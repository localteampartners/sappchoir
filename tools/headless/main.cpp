// sappchoir-headless — the station harness.
//
// Drives SappChoirProcessor exactly the way the sappradio station host does:
// no editor is ever created, and — this is the part that mattered — the JUCE
// dispatch loop is NEVER run. A plugin embedded in a non-JUCE headless host
// has a MessageManager but nothing pumps it, so juce::Timer callbacks and
// MessageManager::callAsync() never fire. Anything the plugin needs the
// message loop for simply does not happen, silently (sappchoir #1,
// sapporchestra #2).
//
//   sappchoir-headless selftest [--fixture DIR]
//       Regression suite for sappchoir #1. Exit 0 = all pass.
//
//   sappchoir-headless render --instrument LABEL [--out F.wav]
//                             [--root DIR] [--settle MS] [--pump]
//                             [--param ID=VALUE ...]
//       One station-style render: sustained D3-D5 chords, NO CCs, default
//       parameters. --pump runs the dispatch loop during the settle window
//       (i.e. pretends to be a JUCE host); the default does not, which is
//       the real station condition.

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

#include <juce_audio_utils/juce_audio_utils.h>

#include "PluginProcessor.h"

namespace {

void setEnv(const char* name, const juce::String& value)
{
#if JUCE_WINDOWS
    _putenv_s(name, value.toRawUTF8());
#else
    if (value.isEmpty()) ::unsetenv(name);
    else ::setenv(name, value.toRawUTF8(), 1);
#endif
}

double toDb(double linear)
{
    return linear > 1.0e-12 ? 20.0 * std::log10(linear) : -200.0;
}

struct RenderResult {
    std::vector<float> left, right;
    juce::String instrumentPath, instrumentName, status;
    bool libraryReady = false;
    double rms = 0.0, peak = 0.0;
    int maxVoices = 0;          // peak active voice count during the render
    uint64_t hash = 0;
};

uint64_t audioHash(const std::vector<float>& l, const std::vector<float>& r)
{
    uint64_t h = 1469598103934665603ull;
    auto feed = [&h](float v) {
        uint32_t bits;
        std::memcpy(&bits, &v, sizeof(bits));
        for (int b = 0; b < 4; ++b) {
            h ^= uint64_t((bits >> (b * 8)) & 0xff);
            h *= 1099511628211ull;
        }
    };
    for (float v : l) feed(v);
    for (float v : r) feed(v);
    return h;
}

int choiceForLabel(const sappchoir::SappChoirProcessor& processor,
                   const juce::String& label)
{
    const auto& library = processor.sfzLibrary();
    for (size_t i = 0; i < library.size(); ++i)
        if (juce::String::fromUTF8(library[i].label.c_str()) == label)
            return int(i) + 1;   // choice 0 = "(keep current)"
    return -1;
}

struct RenderOptions {
    int settleMs = 4000;
    bool pump = false;
    juce::StringPairArray params;      // parameter id -> engineering value
    juce::Array<int> cc;               // pairs: number, value (sent at frame 0)
};

// The station's shape: sustained D3..D5 chords, four chords, no CC traffic.
struct ChordEvent { int block; bool on; int note; };

std::vector<ChordEvent> stationScore(int blocksPerChord, int chords)
{
    static const int voicings[4][4] = {
        {50, 57, 62, 65},   // D3 A3 D4 F4
        {53, 60, 65, 69},
        {55, 62, 67, 71},
        {50, 57, 62, 74},   // ... up to D5
    };
    std::vector<ChordEvent> score;
    for (int c = 0; c < chords; ++c) {
        const int start = c * blocksPerChord;
        for (int v = 0; v < 4; ++v) {
            score.push_back({start, true, voicings[c % 4][v]});
            score.push_back({start + blocksPerChord - 8, false, voicings[c % 4][v]});
        }
    }
    return score;
}

// One station render. `label` empty = select nothing (the control case).
RenderResult stationRender(const juce::String& label, const RenderOptions& options,
                           bool* labelResolved = nullptr)
{
    RenderResult out;
    auto processor = std::make_unique<sappchoir::SappChoirProcessor>();
    processor->prepareToPlay(48000.0, 512);

    if (label.isNotEmpty()) {
        const int choice = choiceForLabel(*processor, label);
        if (labelResolved != nullptr) *labelResolved = choice > 0;
        if (choice > 0) {
            auto* parameter = processor->valueTree().getParameter("instrument");
            // Exactly what a host does with a display name: normalized write.
            parameter->setValueNotifyingHost(parameter->convertTo0to1(float(choice)));
        }
    } else if (labelResolved != nullptr) {
        *labelResolved = true;
    }

    for (const auto& id : options.params.getAllKeys())
        if (auto* parameter = processor->valueTree().getParameter(id))
            parameter->setValueNotifyingHost(
                parameter->convertTo0to1(options.params[id].getFloatValue()));

    // Settle window. The station passes --settle and does NOT pump a JUCE
    // dispatch loop; --pump models a JUCE-based host instead.
    const auto deadline = juce::Time::getMillisecondCounter() + uint32_t(options.settleMs);
    while (juce::Time::getMillisecondCounter() < deadline) {
        if (processor->libraryReady()) break;
        if (options.pump) juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
        else juce::Thread::sleep(5);
    }

    out.instrumentPath = processor->currentInstrumentPath();
    out.instrumentName = processor->currentInstrumentName();
    out.status = processor->loadStatus();
    out.libraryReady = processor->libraryReady();

    constexpr int kBlock = 512;
    constexpr int kBlocksPerChord = 180;              // ~1.9 s at 48 kHz
    const auto score = stationScore(kBlocksPerChord, 4);
    const int kBlocks = kBlocksPerChord * 4 + 200;    // + tail

    juce::AudioBuffer<float> buffer(2, kBlock);
    for (int b = 0; b < kBlocks; ++b) {
        juce::MidiBuffer midi;
        if (b == 0)
            for (int i = 0; i + 1 < options.cc.size(); i += 2)
                midi.addEvent(juce::MidiMessage::controllerEvent(1, options.cc[i],
                                                                 options.cc[i + 1]), 0);
        for (const auto& e : score)
            if (e.block == b)
                midi.addEvent(e.on ? juce::MidiMessage::noteOn(1, e.note, 0.75f)
                                   : juce::MidiMessage::noteOff(1, e.note), 0);
        buffer.clear();
        processor->processBlock(buffer, midi);
        out.maxVoices = juce::jmax(out.maxVoices,
                                   processor->engine().sampler().activeVoiceCount());
        out.left.insert(out.left.end(), buffer.getReadPointer(0),
                        buffer.getReadPointer(0) + kBlock);
        out.right.insert(out.right.end(), buffer.getReadPointer(1),
                         buffer.getReadPointer(1) + kBlock);
    }

    double sum = 0.0;
    for (size_t i = 0; i < out.left.size(); ++i) {
        sum += double(out.left[i]) * out.left[i] + double(out.right[i]) * out.right[i];
        out.peak = std::max(out.peak, double(std::abs(out.left[i])));
        out.peak = std::max(out.peak, double(std::abs(out.right[i])));
    }
    out.rms = std::sqrt(sum / double(out.left.size() * 2));
    out.hash = audioHash(out.left, out.right);
    processor.reset();
    return out;
}

// --------------------------------------------------------------- selftest --

int fails = 0;

void check(bool ok, const juce::String& what)
{
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8());
    std::fflush(stdout);
    if (!ok) ++fails;
}

juce::File prepareFixture(const juce::String& fixtureRoot, const juce::String& name)
{
    const juce::File source(fixtureRoot);
    const auto root = juce::File::getSpecialLocation(juce::File::tempDirectory)
                          .getChildFile(name);
    root.deleteRecursively();
    if (!source.isDirectory() || !source.copyDirectoryTo(root)) {
        std::printf("FAIL: cannot copy fixture %s\n", fixtureRoot.toRawUTF8());
        ++fails;
        return {};
    }
    root.getChildFile(sapp::sfzlib::kIndexFileName).deleteFile();
    return root;
}

// The usable band. A choir chain that the station can air sits well above
// -45 dBFS RMS and below clipping; the reported fault was -61.2 dBFS.
constexpr double kUsableRmsDbLo = -45.0;
constexpr double kUsableRmsDbHi = -6.0;

int runSelftest(const juce::String& fixtureRoot)
{
    const auto root = prepareFixture(fixtureRoot, "sappchoir-headless");
    if (!root.isDirectory()) return 1;
    setEnv(sapp::sfzlib::kRootEnvVar, root.getFullPathName());

    RenderOptions plain;
    plain.settleMs = 8000;

    std::printf("sappchoir #1 — a default headless instance must be audible\n");

    // ---- the built-in default sounds with no host assistance at all -------
    const auto builtin = stationRender({}, plain);
    std::printf("        built-in: rms %.6f (%.1f dBFS) peak %.4f voices %d "
                "status \"%s\" ready %d\n",
                builtin.rms, toDb(builtin.rms), builtin.peak, builtin.maxVoices,
                builtin.status.toRawUTF8(), builtin.libraryReady ? 1 : 0);
    check(builtin.maxVoices > 0, "voices actually render with no host assistance");
    check(toDb(builtin.rms) > kUsableRmsDbLo && toDb(builtin.rms) < kUsableRmsDbHi,
          "the built-in default renders in the usable band "
          "(-45..-6 dBFS RMS): " + juce::String(toDb(builtin.rms), 1) + " dBFS");
    check(builtin.libraryReady, "libraryReady reads 1 without a dispatch loop");

    // ---- a selected SFZ loads and sounds, headlessly ----------------------
    bool resolved = false;
    const auto selected = stationRender("loud/Loud Choir", plain, &resolved);
    std::printf("        selected: rms %.6f (%.1f dBFS) peak %.4f voices %d "
                "loaded \"%s\"\n",
                selected.rms, toDb(selected.rms), selected.peak, selected.maxVoices,
                selected.instrumentPath.toRawUTF8());
    check(resolved, "the label resolved to a choice index");
    check(selected.instrumentPath ==
              root.getChildFile("loud").getChildFile("Loud Choir.sfz").getFullPathName(),
          "the selected SFZ is the one that loaded");
    check(selected.hash != builtin.hash,
          "selected and unselected renders DIFFER");
    check(toDb(selected.rms) > kUsableRmsDbLo && toDb(selected.rms) < kUsableRmsDbHi,
          "the selected instrument renders in the usable band: "
              + juce::String(toDb(selected.rms), 1) + " dBFS");
    check(selected.libraryReady, "libraryReady reads 1 once the selection is installed");

    // ---- a different selection really is a different sound ----------------
    const auto quiet = stationRender("quiet/Quiet Choir", plain);
    check(quiet.instrumentPath ==
              root.getChildFile("quiet").getChildFile("Quiet Choir.sfz").getFullPathName(),
          "the second label loaded its own SFZ");
    check(quiet.rms < selected.rms * 0.6,
          "the quiet instrument really is the quiet one (it is what sounded)");

    // ---- a JUCE-style host that DOES pump must still work -----------------
    RenderOptions pumped = plain;
    pumped.pump = true;
    const auto withPump = stationRender("loud/Loud Choir", pumped);
    check(withPump.hash == selected.hash,
          "pumping the message loop changes nothing (same render)");

    // ---- no parameter may default to silence ------------------------------
    // `clean` is the suite-wide imperfection scaler: 0 = fully modeled.
    // At either extreme the instrument must still sound.
    RenderOptions fullyClean = plain;
    fullyClean.params.set("clean", "1.0");
    const auto cleaned = stationRender("loud/Loud Choir", fullyClean);
    check(toDb(cleaned.rms) > kUsableRmsDbLo,
          "clean=1 still renders in the usable band: "
              + juce::String(toDb(cleaned.rms), 1) + " dBFS");
    check(cleaned.hash != selected.hash, "clean=1 changes the sound (breath scales out)");

    // ---- state restore installs the instrument headlessly -----------------
    {
        auto settle = [](sappchoir::SappChoirProcessor& p) {
            const auto deadline = juce::Time::getMillisecondCounter() + 8000u;
            while (juce::Time::getMillisecondCounter() < deadline && p.isLoading())
                juce::Thread::sleep(5);   // still no dispatch loop anywhere
        };
        auto saved = std::make_unique<sappchoir::SappChoirProcessor>();
        saved->prepareToPlay(48000.0, 512);
        saved->loadSfzInstrument(root.getChildFile("loud").getChildFile("Loud Choir.sfz"));
        settle(*saved);
        juce::MemoryBlock state;
        saved->getStateInformation(state);
        saved.reset();

        auto restored = std::make_unique<sappchoir::SappChoirProcessor>();
        restored->prepareToPlay(48000.0, 512);
        restored->setStateInformation(state.getData(), int(state.getSize()));
        settle(*restored);
        check(restored->currentInstrumentPath() ==
                  root.getChildFile("loud").getChildFile("Loud Choir.sfz").getFullPathName(),
              "a state restore installed its SFZ headlessly");
        restored.reset();
    }

    std::printf("selftest: %s\n", fails == 0 ? "ALL PASS" : "FAILURES");
    return fails == 0 ? 0 : 1;
}

} // namespace

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    const juce::String command = argc > 1 ? juce::String(argv[1]) : juce::String();
    juce::String root, instrument, out, fixture;
    RenderOptions options;
    for (int i = 2; i < argc; ++i) {
        const juce::String arg(argv[i]);
        auto next = [&]() -> juce::String {
            return i + 1 < argc ? juce::String(argv[++i]) : juce::String();
        };
        if (arg == "--root") root = next();
        else if (arg == "--fixture") fixture = next();
        else if (arg == "--instrument") instrument = next();
        else if (arg == "--out") out = next();
        else if (arg == "--settle") options.settleMs = next().getIntValue();
        else if (arg == "--pump") options.pump = true;
        else if (arg == "--param") {
            const auto kv = next();
            options.params.set(kv.upToFirstOccurrenceOf("=", false, false),
                               kv.fromFirstOccurrenceOf("=", false, false));
        } else if (arg == "--cc") {
            const auto kv = next();
            options.cc.add(kv.upToFirstOccurrenceOf("=", false, false).getIntValue());
            options.cc.add(kv.fromFirstOccurrenceOf("=", false, false).getIntValue());
        }
    }

    if (command == "selftest") {
        if (fixture.isEmpty()) fixture = root;
#ifdef SAPPCHOIR_TEST_DATA_DIR
        if (fixture.isEmpty()) fixture = juce::String(SAPPCHOIR_TEST_DATA_DIR) + "/sfz-headless";
#endif
        return runSelftest(fixture);
    }

    if (command == "render") {
        if (root.isNotEmpty())
            setEnv(sapp::sfzlib::kRootEnvVar, root);
        const auto result = stationRender(instrument, options);
        std::printf("instrument: %s\n", result.instrumentPath.toRawUTF8());
        std::printf("name:       %s\n", result.instrumentName.toRawUTF8());
        std::printf("status:     %s\n", result.status.toRawUTF8());
        std::printf("ready:      %d\n", result.libraryReady ? 1 : 0);
        std::printf("voices:     %d\n", result.maxVoices);
        std::printf("rms:        %.8f  (%.2f dBFS)\n", result.rms, toDb(result.rms));
        std::printf("peak:       %.8f  (%.2f dBFS)\n", result.peak, toDb(result.peak));
        std::printf("hash:       %016llx\n", (unsigned long long) result.hash);
        if (out.isNotEmpty()) {
            juce::File file(out);
            file.deleteFile();
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::FileOutputStream> stream(file.createOutputStream());
            if (stream != nullptr) {
                std::unique_ptr<juce::AudioFormatWriter> writer(
                    wav.createWriterFor(stream.get(), 48000.0, 2, 24, {}, 0));
                if (writer != nullptr) {
                    stream.release();
                    const float* channels[2] = {result.left.data(), result.right.data()};
                    writer->writeFromFloatArrays(channels, 2, int(result.left.size()));
                }
            }
            std::printf("wrote:      %s\n", out.toRawUTF8());
        }
        return 0;
    }

    std::fprintf(stderr,
                 "sappchoir-headless — station harness (no GUI, no message loop)\n"
                 "  sappchoir-headless selftest [--fixture DIR]\n"
                 "  sappchoir-headless render   --instrument LABEL [--out F.wav]\n"
                 "                              [--root DIR] [--settle MS] [--pump]\n"
                 "                              [--param ID=VALUE] [--cc N=V]\n");
    return 2;
}
