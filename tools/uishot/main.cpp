// SappChoirUiShot — renders the plugin editor offscreen and writes a PNG.
// Used to verify UI changes without a screen-recording session.
//   SappChoirUiShot [output.png]
//   SappChoirUiShot --sounds [output.png]   (GET SOUNDS overlay open)
//   SappChoirUiShot --cctest    (SappLink plugin-path proof: CC 20 morphs)

#include <cmath>
#include <cstdlib>
#include <functional>
#include <iterator>

#include <juce_audio_utils/juce_audio_utils.h>

#include "PluginProcessor.h"

class UiShotApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "SappChoirUiShot"; }
    const juce::String getApplicationVersion() override { return "1.0"; }

    // --sfztest <fixture-root>: headless proof of the `instrument` choice
    // parameter (sapptune issue #20). Copies the fixture library to a temp
    // dir, points SAPP_SFZ_ROOT at it, and asserts: the choice list is the
    // case-insensitively sorted library; selecting choice N loads entry N-1;
    // MIDI bank-select + program change loads by entry index; the chosen SFZ
    // round-trips through host state BY PATH; and no pre-existing parameter
    // moved (the new parameter is appended last).
    int sfzFails = 0;

    void sfzCheck(bool ok, const juce::String& what)
    {
        std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8());
        if (!ok) ++sfzFails;
    }

    bool pumpUntil(const std::function<bool()>& done, int timeoutMs)
    {
        const auto deadline = juce::Time::getMillisecondCounter() + uint32_t(timeoutMs);
        while (juce::Time::getMillisecondCounter() < deadline) {
            if (done()) return true;
            juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
        }
        return done();
    }

    void runSfzTest(const juce::String& fixtureRoot)
    {
        // Work on a copy so the index-file cache never pollutes tests/data.
        const juce::File src(fixtureRoot);
        const juce::File root =
            juce::File::getSpecialLocation(juce::File::tempDirectory)
                .getChildFile("sappchoir-sfztest");
        root.deleteRecursively();
        if (!src.isDirectory() || !src.copyDirectoryTo(root)) {
            std::printf("FAIL: cannot copy fixture %s\n", fixtureRoot.toRawUTF8());
            setApplicationReturnValue(1);
            quit();
            return;
        }
        ::setenv(sapp::sfzlib::kRootEnvVar, root.getFullPathName().toRawUTF8(), 1);

        processor = std::make_unique<sappchoir::SappChoirProcessor>();
        processor->prepareToPlay(48000.0, 512);
        pumpUntil([this] { return !processor->isLoading(); }, 10000);  // built-in load

        // --- A. parameter table: existing order intact, additions appended --
        // `instrument` (sapptune #20), then `clean` (CC 3, sappchoir #1), then
        // the read-only `libraryReady` readout. Every pre-existing automation
        // index holds because nothing is ever inserted before them.
        const char* expectedIds[] = {"vowel", "dynamics", "expression", "breath",
                                     "ensemble", "width", "earlyLevel", "tailLevel",
                                     "spaceSize", "spaceDecay", "spaceDamping",
                                     "legato", "masterGain", "limiter", "quality",
                                     "articulation", "instrument", "clean",
                                     "libraryReady"};
        const auto& params = processor->getParameters();
        bool tableOk = params.size() == int(std::size(expectedIds));
        for (int i = 0; tableOk && i < params.size(); ++i) {
            auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*>(params[i]);
            tableOk = withId != nullptr && withId->paramID == expectedIds[i];
            if (!tableOk)
                std::printf("  param %d is %s, expected %s\n", i,
                            withId ? withId->paramID.toRawUTF8() : "?", expectedIds[i]);
        }
        sfzCheck(tableOk,
                 "16 pre-existing params unchanged; instrument/clean/libraryReady appended");

        // --- B. choice list mirrors the sorted library ---------------------
        auto* choice = dynamic_cast<juce::AudioParameterChoice*>(
            processor->valueTree().getParameter("instrument"));
        sfzCheck(choice != nullptr, "`instrument` is an AudioParameterChoice");
        if (choice == nullptr) { finishSfzTest(); return; }
        const auto& library = processor->sfzLibrary();
        sfzCheck(library.size() == 3 && choice->choices.size() == 4,
                 "fixture scan: 3 instruments + \"(keep current)\" = 4 choices");
        sfzCheck(choice->choices[0] == "(keep current)", "choice 0 keeps the current sound");
        sfzCheck(choice->choices[1] == "acme/Beta Flute"
                     && choice->choices[2] == "Gamma"
                     && choice->choices[3] == "Zeta Lib/alpha trumpet",
                 "choices sorted case-insensitively (Beta < Gamma < alpha-in-Zeta)");

        // --- C. selecting choice 2 loads entry 1 ("Gamma") -----------------
        choice->setValueNotifyingHost(choice->convertTo0to1(2.0f));
        const juce::String gammaPath = root.getChildFile("Gamma.sfz").getFullPathName();
        sfzCheck(pumpUntil([&] { return processor->currentInstrumentPath() == gammaPath; }, 10000),
                 "choice 2 loaded " + gammaPath);

        // --- D. MIDI bank select + program change --------------------------
        {
            juce::AudioBuffer<float> buffer(2, 512);
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::controllerEvent(1, 0, 0), 0);    // bank MSB
            midi.addEvent(juce::MidiMessage::controllerEvent(1, 32, 0), 1);   // bank LSB
            midi.addEvent(juce::MidiMessage::programChange(1, 0), 2);         // entry 0
            processor->processBlock(buffer, midi);
        }
        const juce::String betaPath =
            root.getChildFile("acme").getChildFile("Beta Flute.sfz").getFullPathName();
        sfzCheck(pumpUntil([&] { return processor->currentInstrumentPath() == betaPath; }, 10000),
                 "bank 0 / program 0 loaded entry 0: " + betaPath);
        sfzCheck(pumpUntil([&] { return int(choice->getIndex()) == 1; }, 2000),
                 "`instrument` parameter re-synced to choice 1 after program change");

        // --- E. state round-trip BY PATH -----------------------------------
        auto* spaceSize = processor->valueTree().getParameter("spaceSize");
        spaceSize->setValueNotifyingHost(spaceSize->convertTo0to1(1.25f));
        juce::MemoryBlock state;
        processor->getStateInformation(state);

        auto second = std::make_unique<sappchoir::SappChoirProcessor>();
        second->prepareToPlay(48000.0, 512);
        second->setStateInformation(state.getData(), int(state.getSize()));
        sfzCheck(pumpUntil([&] { return second->currentInstrumentPath() == betaPath; }, 10000),
                 "state restore reloaded the chosen SFZ by path");
        auto* spaceSize2 = second->valueTree().getParameter("spaceSize");
        sfzCheck(std::abs(spaceSize2->convertFrom0to1(spaceSize2->getValue()) - 1.25f) < 0.01f,
                 "pre-existing parameter (spaceSize) recalled identically");
        auto* choice2 = dynamic_cast<juce::AudioParameterChoice*>(
            second->valueTree().getParameter("instrument"));
        sfzCheck(pumpUntil([&] { return choice2->getIndex() == 1; }, 2000),
                 "restored instance re-synced `instrument` to the loaded path");
        second.reset();

        finishSfzTest();
    }

    void finishSfzTest()
    {
        std::printf("sfztest: %s\n", sfzFails == 0 ? "ALL PASS" : "FAILURES");
        processor.reset();
        setApplicationReturnValue(sfzFails == 0 ? 0 : 1);
        quit();
    }

    // --cctest: end-to-end SappLink proof through the PLUGIN path — CC 20
    // (vowel) arrives via processBlock, slews the APVTS parameter exactly
    // like host automation, forwards to the sampler's crossfade layers, and
    // must brighten the rendered spectrum (oo -> eh).
    void runCcTest()
    {
        processor = std::make_unique<sappchoir::SappChoirProcessor>();
        processor->prepareToPlay(48000.0, 512);
        // The built-in instrument loads asynchronously on the message
        // thread; give it (and the vowel-layer generation) time, then measure.
        juce::Timer::callAfterDelay(4000, [this] { finishCcTest(); });
    }

    void finishCcTest()
    {
        // Kill the room so the direct path dominates the measurement.
        processor->valueTree().getParameter("tailLevel")->setValueNotifyingHost(0.0f);
        processor->valueTree().getParameter("earlyLevel")->setValueNotifyingHost(0.0f);
        processor->valueTree().getParameter("breath")->setValueNotifyingHost(0.0f);

        juce::AudioBuffer<float> buffer(2, 512);
        auto measureHfRatio = [&](int ccValue) {
            double hf = 0.0, total = 0.0;
            float prev = 0.0f;
            for (int b = 0; b < 120; ++b) {   // ~1.3 s per side
                juce::MidiBuffer midi;
                if (b == 0) {
                    midi.addEvent(juce::MidiMessage::controllerEvent(1, 20, ccValue), 0);
                    midi.addEvent(juce::MidiMessage::noteOn(1, 48, 0.8f), 1);
                }
                buffer.clear();
                processor->processBlock(buffer, midi);
                if (b > 40) {  // measure after the slew settles
                    for (int i = 0; i < 512; ++i) {
                        const float v = buffer.getSample(0, i);
                        const float d = v - prev;
                        hf += double(d) * d;
                        total += double(v) * v;
                        prev = v;
                    }
                }
            }
            juce::MidiBuffer off;
            off.addEvent(juce::MidiMessage::allNotesOff(1), 0);
            buffer.clear();
            processor->processBlock(buffer, off);
            for (int b = 0; b < 100; ++b) { juce::MidiBuffer none; buffer.clear(); processor->processBlock(buffer, none); }
            return total > 0.0 ? hf / total : 0.0;
        };

        const double dark = measureHfRatio(0);      // vowel oo
        const double bright = measureHfRatio(127);  // vowel eh
        const bool pass = bright > dark * 1.3;
        std::printf("SappLink CC20 vowel sweep: oo hf %.4g  eh hf %.4g  [%s]\n",
                    dark, bright, pass ? "PASS" : "FAIL");
        editor.reset();
        processor.reset();
        setApplicationReturnValue(pass ? 0 : 1);
        quit();
    }

    void initialise(const juce::String& commandLine) override
    {
        if (commandLine.contains("--cctest")) {
            runCcTest();
            return;
        }
        if (commandLine.contains("--sfztest")) {
            const auto fixture =
                commandLine.fromFirstOccurrenceOf("--sfztest", false, false).trim().unquoted();
            runSfzTest(fixture);
            return;
        }

        const bool openSounds = commandLine.contains("--sounds");
        const juce::String pathArg =
            commandLine.replace("--sounds", "").trim().unquoted();
        const juce::String outPath =
            pathArg.isNotEmpty() ? pathArg : juce::String("/tmp/sappchoir-ui.png");

        processor = std::make_unique<sappchoir::SappChoirProcessor>();
        processor->prepareToPlay(48000.0, 512);
        editor.reset(processor->createEditor());

        // Give the async built-in load (incl. vowel-layer generation) and
        // fonts time to settle, then hold a chord so the meter/voices are
        // alive in the shot.
        juce::Timer::callAfterDelay(4000, [this, outPath, openSounds]
        {
            // --sounds: click the header button so the overlay is in the shot.
            if (openSounds)
                for (auto* child : editor->getChildren())
                    if (auto* button = dynamic_cast<juce::TextButton*>(child))
                        if (button->getButtonText() == "GET SOUNDS")
                            button->triggerClick();

            juce::AudioBuffer<float> buffer(2, 512);
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 48, 0.8f), 0);
            midi.addEvent(juce::MidiMessage::noteOn(1, 55, 0.7f), 0);
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.75f), 0);
            midi.addEvent(juce::MidiMessage::noteOn(1, 64, 0.7f), 0);
            for (int i = 0; i < 20; ++i) {
                buffer.clear();
                processor->processBlock(buffer, midi);
                midi.clear();
            }

            // The sounds overlay scans the samples folder recursively; give
            // that scan time to land before the shot.
            juce::Timer::callAfterDelay(openSounds ? 2500 : 300, [this, outPath]
            {
                auto snapshot = editor->createComponentSnapshot(editor->getLocalBounds(), true, 2.0f);
                juce::File file(outPath);
                file.deleteFile();
                juce::FileOutputStream stream(file);
                juce::PNGImageFormat png;
                if (stream.openedOk() && png.writeImageToStream(snapshot, stream))
                    std::printf("wrote %s (%dx%d)\n", outPath.toRawUTF8(),
                                snapshot.getWidth(), snapshot.getHeight());
                else
                    std::printf("FAILED to write %s\n", outPath.toRawUTF8());
                editor.reset();
                processor.reset();
                quit();
            });
        });
    }

    void shutdown() override {}

private:
    std::unique_ptr<sappchoir::SappChoirProcessor> processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor;
};

START_JUCE_APPLICATION(UiShotApp)
