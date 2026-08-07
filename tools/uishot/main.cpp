// SappChoirUiShot — renders the plugin editor offscreen and writes a PNG.
// Used to verify UI changes without a screen-recording session.
//   SappChoirUiShot [output.png]
//   SappChoirUiShot --sounds [output.png]   (GET SOUNDS overlay open)
//   SappChoirUiShot --cctest    (SappLink plugin-path proof: CC 20 morphs)

#include <juce_audio_utils/juce_audio_utils.h>

#include "PluginProcessor.h"

class UiShotApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "SappChoirUiShot"; }
    const juce::String getApplicationVersion() override { return "1.0"; }

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
