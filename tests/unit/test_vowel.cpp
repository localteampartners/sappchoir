// The signature feature: vowel morphing via SappSounds' live CC crossfade
// engine, with generated formant layers when the library has none.

#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include <sapp/sounds/PlaybackEngine.h>

#include "ChoirTestHelpers.h"
#include "core/VowelLayers.h"

using namespace sapp::choir;
using namespace sapp::sounds;
using namespace sappchoirtest;

TEST_CASE("formant filter imposes the vowel's spectral shape", "[vowel]")
{
    // Harmonic-rich saw through 'oo' vs 'eh': the F2 region must swap weight.
    const SampleData saw = makeSawSample(100.0, 48000, 1.0);
    std::vector<float> oo(saw.data[0].size()), eh(saw.data[0].size());
    formantFilter(saw.data[0].data(), oo.data(), oo.size(), 48000.0, vowelFormants(0));
    formantFilter(saw.data[0].data(), eh.data(), eh.size(), 48000.0, vowelFormants(3));

    // 'eh' has F2 ~1850 Hz; 'oo' has F2 ~870 Hz.
    const double ooLow = bandEnergy(oo, 48000, 900.0, 4800, 43200);
    const double ooHigh = bandEnergy(oo, 48000, 1900.0, 4800, 43200);
    const double ehLow = bandEnergy(eh, 48000, 900.0, 4800, 43200);
    const double ehHigh = bandEnergy(eh, 48000, 1900.0, 4800, 43200);
    CHECK(ooLow > ooHigh * 2.0);            // oo is dark
    CHECK(ehHigh > ooHigh * 3.0);           // eh's F2 region jumps out
    CHECK(ehHigh / (ehLow + 1e-12) > ooHigh / (ooLow + 1e-12) * 3.0);

    // RMS-matched: refiltering must not change loudness by more than ~2 dB.
    auto rms = [](const std::vector<float>& x) {
        double s = 0.0;
        for (float v : x) s += double(v) * v;
        return std::sqrt(s / double(x.size()));
    };
    const double sawRms = rms(saw.data[0]);
    CHECK(rms(oo) > sawRms * 0.8);
    CHECK(rms(oo) < sawRms * 1.26);
    CHECK(rms(eh) > sawRms * 0.8);
    CHECK(rms(eh) < sawRms * 1.26);
}

TEST_CASE("formant filtering is deterministic", "[vowel]")
{
    const SampleData saw = makeSawSample(100.0, 48000, 0.25);
    std::vector<float> a(saw.data[0].size()), b(saw.data[0].size());
    formantFilter(saw.data[0].data(), a.data(), a.size(), 48000.0, vowelFormants(2));
    formantFilter(saw.data[0].data(), b.data(), b.size(), 48000.0, vowelFormants(2));
    REQUIRE(a == b);
}

TEST_CASE("makeVowelInstrument quadruples attack regions with crossfade windows", "[vowel]")
{
    auto base = makeSawInstrument();
    auto morph = makeVowelInstrument(base, kVowelCc);

    REQUIRE(morph != nullptr);
    REQUIRE(morph != base);
    REQUIRE(morph->definition.regions.size() == 4);
    REQUIRE(morph->samples.size() == 5);  // original + 4 vowel copies
    CHECK(hasVowelLayers(morph->definition, kVowelCc));

    for (int v = 0; v < kNumVowels; ++v) {
        const auto& region = morph->definition.regions[size_t(v)];
        REQUIRE(region.ccCrossfades.size() == 1);
        const auto& crossfade = region.ccCrossfades[0];
        CHECK(int(crossfade.cc) == kVowelCc);
        const auto window = vowelWindow(v);
        CHECK(crossfade.inLo == window.inLo);
        CHECK(crossfade.inHi == window.inHi);
        CHECK(crossfade.outLo == window.outLo);
        CHECK(crossfade.outHi == window.outHi);
        // Loop metadata carries over — pads must sustain.
        CHECK(region.loop.mode == LoopMode::Continuous);
    }

    // Adjacent windows tile the CC range: every CC value reaches some layer.
    // (oo covers 0.., eh covers ..127, and each in-range matches the
    // previous layer's out-range.)
    CHECK(vowelWindow(0).outLo == vowelWindow(1).inLo);
    CHECK(vowelWindow(0).outHi == vowelWindow(1).inHi);
    CHECK(vowelWindow(1).outLo == vowelWindow(2).inLo);
    CHECK(vowelWindow(1).outHi == vowelWindow(2).inHi);
    CHECK(vowelWindow(2).outLo == vowelWindow(3).inLo);
    CHECK(vowelWindow(2).outHi == vowelWindow(3).inHi);
}

TEST_CASE("makeVowelInstrument is idempotent on morph-ready instruments", "[vowel]")
{
    auto base = makeSawInstrument();
    auto morph = makeVowelInstrument(base, kVowelCc);
    auto again = makeVowelInstrument(morph, kVowelCc);
    CHECK(again == morph);  // library-authored (or generated) layers pass through
}

TEST_CASE("vowel CC morphs the sound while the note is held", "[vowel]")
{
    auto morph = makeVowelInstrument(makeSawInstrument(), kVowelCc);

    PlaybackEngine engine;
    engine.prepare(48000, 512);
    engine.setInstrument(morph);

    // Hold A2 (root: no resampling); sweep CC20 0 -> 127 mid-note.
    std::vector<MidiEvent> events{controller(0, uint8_t(kVowelCc), 0), noteOn(10, 45, 100)};
    for (int i = 0; i <= 16; ++i)
        events.push_back(controller(uint32_t(48000 + i * 1000), uint8_t(kVowelCc),
                                    uint8_t(i * 127 / 16)));

    const int total = 144000;
    std::vector<float> left(size_t(total), 0.0f), right(size_t(total), 0.0f);
    size_t next = 0;
    for (int start = 0; start < total; start += 512) {
        const int frames = std::min(512, total - start);
        std::vector<MidiEvent> blockEvents;
        while (next < events.size() && events[next].frame < uint32_t(start + frames)) {
            MidiEvent e = events[next];
            e.frame = e.frame - uint32_t(start);
            blockEvents.push_back(e);
            ++next;
        }
        engine.process(blockEvents.data(), int(blockEvents.size()),
                       left.data() + start, right.data() + start, frames);
    }

    // All four vowel layers sound as one note.
    CHECK(engine.activeVoiceCount() == 4);

    // Early (CC 0, oo): dark. Late (CC 127, eh): F2 band alive.
    const double earlyHigh = bandEnergy(left, 48000, 1900.0, 8000, 44000);
    const double earlyLow = bandEnergy(left, 48000, 900.0, 8000, 44000);
    const double lateHigh = bandEnergy(left, 48000, 1900.0, 100000, 140000);
    const double lateLow = bandEnergy(left, 48000, 900.0, 100000, 140000);
    CHECK(earlyLow > earlyHigh * 2.0);
    CHECK(lateHigh / (lateLow + 1e-12) > earlyHigh / (earlyLow + 1e-12) * 2.0);

    // The morph is smooth: the saw's own period edge dominates the
    // sample-to-sample jumps. During the sweep two adjacent layers overlap at
    // equal-power gains, so the edge may grow by up to ~sqrt(2) versus the
    // steady bright layer — anything much larger would be a crossfade click.
    auto maxJump = [&](size_t a, size_t b) {
        float jump = 0.0f;
        for (size_t i = a; i + 1 < b && i + 1 < left.size(); ++i)
            jump = std::max(jump, std::abs(left[i + 1] - left[i]));
        return jump;
    };
    const float steadyEh = maxJump(100000, 144000);  // after the sweep (eh)
    const float sweeping = maxJump(48000, 100000);
    CHECK(sweeping < steadyEh * 1.6f);
}
