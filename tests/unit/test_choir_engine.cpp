#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

#include <sapp/sounds/DiagnosticInstrument.h>

#include "ChoirTestHelpers.h"
#include "core/ChoirEngine.h"
#include "core/VowelLayers.h"

using namespace sapp::choir;
using sapp::sounds::MidiEvent;
using namespace sappchoirtest;

namespace {

ChoirEngine& freshEngine(ChoirEngine& engine, ChoirParams params = {})
{
    engine.prepare(48000, 512);
    engine.setParams(params);
    engine.setInstrument(makeVowelInstrument(makeSawInstrument(), kVowelCc));
    return engine;
}

} // namespace

TEST_CASE("engine produces sound with cathedral tail", "[choir]")
{
    ChoirEngine engine;
    freshEngine(engine);
    auto out = run(engine, {noteOn(0, 45, 100), noteOff(24000, 45)}, 96000);
    CHECK(out.peak > 0.02f);
    // The note stops at 0.5 s; the cathedral keeps ringing well after release.
    float lateRms = 0.0f;
    for (size_t i = 48000; i < out.left.size(); ++i) lateRms += std::abs(out.left[i]);
    CHECK(lateRms / 48000.0f > 1.0e-4f);
}

TEST_CASE("CC1 dynamics changes level and brightness", "[choir]")
{
    ChoirEngine a, b;
    freshEngine(a);
    freshEngine(b);

    auto quiet = run(a, {controller(0, 1, 8), noteOn(10, 45, 100)}, 48000);
    auto loud = run(b, {controller(0, 1, 127), noteOn(10, 45, 100)}, 48000);
    CHECK(loud.rms > quiet.rms * 2.0f);  // pp is ~18 dB below ff

    // Brightness must rise with dynamics.
    CHECK(hfRatio(loud.left) > hfRatio(quiet.left) * 1.15);
}

TEST_CASE("CC11 expression scales level", "[choir]")
{
    ChoirEngine a, b;
    freshEngine(a);
    freshEngine(b);
    auto full = run(a, {controller(0, 11, 127), noteOn(10, 45, 100)}, 24000);
    auto pulled = run(b, {controller(0, 11, 40), noteOn(10, 45, 100)}, 24000);
    CHECK(full.rms > pulled.rms * 3.0f);
}

TEST_CASE("vowel parameter reaches the sampler as the Vowel CC", "[choir]")
{
    // No CC events at all: setting the parameter must morph the timbre,
    // because the engine injects the Vowel CC for the crossfade layers.
    ChoirParams oo;
    oo.vowel = 0.0f;
    oo.tailLevel = 0.0f;
    oo.earlyLevel = 0.0f;
    oo.breath = 0.0f;
    ChoirParams eh = oo;
    eh.vowel = 1.0f;

    ChoirEngine a, b;
    freshEngine(a, oo);
    freshEngine(b, eh);
    auto ooOut = run(a, {noteOn(0, 45, 100)}, 48000);
    auto ehOut = run(b, {noteOn(0, 45, 100)}, 48000);

    const double ooHigh = bandEnergy(ooOut.left, 48000, 1900.0, 8000, 44000);
    const double ooLow = bandEnergy(ooOut.left, 48000, 900.0, 8000, 44000);
    const double ehHigh = bandEnergy(ehOut.left, 48000, 1900.0, 8000, 44000);
    const double ehLow = bandEnergy(ehOut.left, 48000, 900.0, 8000, 44000);
    CHECK(ooLow > ooHigh * 2.0);  // oo: dark
    CHECK(ehHigh / (ehLow + 1e-12) > ooHigh / (ooLow + 1e-12) * 2.0);  // eh: F2 up
}

TEST_CASE("incoming Vowel CC overrides and is not fought by the parameter", "[choir]")
{
    // Param says oo, but a live CC20=127 arrives: the CC wins for the rest
    // of the note (engine must not re-inject the stale parameter value).
    ChoirParams p;
    p.vowel = 0.0f;
    p.tailLevel = 0.0f;
    p.earlyLevel = 0.0f;
    p.breath = 0.0f;
    ChoirEngine engine;
    freshEngine(engine, p);
    auto out = run(engine, {noteOn(0, 45, 100), controller(24000, uint8_t(kVowelCc), 127)},
                   96000);

    const double lateHigh = bandEnergy(out.left, 48000, 1900.0, 60000, 92000);
    const double lateLow = bandEnergy(out.left, 48000, 900.0, 60000, 92000);
    const double earlyHigh = bandEnergy(out.left, 48000, 1900.0, 4000, 22000);
    const double earlyLow = bandEnergy(out.left, 48000, 900.0, 4000, 22000);
    CHECK(lateHigh / (lateLow + 1e-12) > earlyHigh / (earlyLow + 1e-12) * 2.0);
}

TEST_CASE("breath adds air to the sustain and stays silent without notes", "[choir]")
{
    ChoirParams dry;
    dry.breath = 0.0f;
    dry.tailLevel = 0.0f;
    dry.earlyLevel = 0.0f;
    ChoirParams airy = dry;
    airy.breath = 1.0f;

    ChoirEngine a, b;
    freshEngine(a, dry);
    freshEngine(b, airy);
    auto plain = run(a, {noteOn(0, 45, 100)}, 48000);
    auto breathy = run(b, {noteOn(0, 45, 100)}, 48000);
    CHECK(hfRatio(breathy.left, 24000, 48000) > hfRatio(plain.left, 24000, 48000) * 1.3);

    // The breath bed follows the choir's envelope: no notes, no hiss.
    ChoirEngine c;
    freshEngine(c, airy);
    auto silence = run(c, {}, 24000);
    CHECK(silence.rms < 1.0e-5f);
}

TEST_CASE("ensemble size engages deterministic per-note detune", "[choir]")
{
    ChoirParams small;
    small.ensemble = 0.0f;
    ChoirParams large = small;
    large.ensemble = 1.0f;

    // Same seed: bit-identical. Different seed: the large ensemble breathes
    // differently (per-note random detune), the single voice does not.
    auto render = [&](const ChoirParams& params, uint32_t seed) {
        ChoirEngine engine;
        engine.prepare(48000, 512);
        engine.setParams(params);
        engine.setInstrument(makeVowelInstrument(makeSawInstrument(), kVowelCc));
        engine.reseed(seed);
        return run(engine, {noteOn(0, 45, 100), noteOn(0, 52, 100), noteOn(0, 57, 100)},
                   24000);
    };

    auto largeA = render(large, 1);
    auto largeA2 = render(large, 1);
    auto largeB = render(large, 2);
    REQUIRE(largeA.left == largeA2.left);

    double diffLarge = 0.0;
    for (size_t i = 0; i < largeA.left.size(); ++i)
        diffLarge += std::abs(double(largeA.left[i]) - largeB.left[i]);
    CHECK(diffLarge > 1.0e-2);

    auto smallA = render(small, 1);
    auto smallB = render(small, 2);
    double diffSmall = 0.0;
    for (size_t i = 0; i < smallA.left.size(); ++i)
        diffSmall += std::abs(double(smallA.left[i]) - smallB.left[i]);
    CHECK(diffLarge > diffSmall * 10.0);
}

TEST_CASE("articulation selection injects the keyswitch", "[choir]")
{
    ChoirEngine engine;
    engine.prepare(48000, 512);
    engine.setParams({});
    engine.setInstrument(sapp::sounds::makeDiagnosticInstrument({48000, 1.2f, 0.5f, 11}));
    run(engine, {}, 512);  // adopt instrument

    engine.selectArticulation(2);  // Pizzicato (keyswitch 14)
    run(engine, {noteOn(0, 60, 100)}, 512);

    sapp::sounds::DiagnosticSnapshot snap;
    REQUIRE(engine.sampler().diagnostics().read(snap));
    CHECK(snap.activeKeyswitch == 14);
}

TEST_CASE("output is always finite, limiter caps extremes", "[choir]")
{
    ChoirParams hot;
    hot.masterGainDb = 12.0f;
    hot.breath = 1.0f;
    hot.ensemble = 1.0f;
    ChoirEngine engine;
    freshEngine(engine, hot);

    std::vector<MidiEvent> wall;
    for (int i = 0; i < 24; ++i) wall.push_back(noteOn(uint32_t(i * 10), uint8_t(36 + i * 2), 127));
    auto out = run(engine, wall, 48000);
    for (float v : out.left) {
        REQUIRE(std::isfinite(v));
        REQUIRE(std::abs(v) <= 1.0f);
    }
}

// --- the suite-wide `clean` control (CC 3, sapptune manifest) --------------

TEST_CASE("clean defaults to 0 — the historical, fully modeled sound", "[choir][clean]")
{
    // A parameter that silences the instrument at its DEFAULT is exactly the
    // shape of bug issue #1 taught us to fear. `clean` defaults to "modeled".
    CHECK(ChoirParams{}.clean == 0.0f);
    CHECK(cleanScale(ChoirParams{}) == 1.0f);
}

TEST_CASE("clean=1 scales the breath-noise bed away", "[choir][clean]")
{
    // Breath noise is gated by the choir's own envelope, so measure the decay
    // tail after release, where the noise bed is the loudest thing left
    // relative to the (silent) sampler output.
    auto tailHf = [](float clean) {
        ChoirParams p;
        p.breath = 1.0f;
        p.ensemble = 0.0f;   // isolate breath from the ensemble humanization
        p.tailLevel = 0.0f;  // and from the reverb, which is not scaled
        p.earlyLevel = 0.0f;
        p.clean = clean;
        ChoirEngine engine;
        freshEngine(engine, p);
        auto out = run(engine, {noteOn(0, 45, 100)}, 48000);
        return hfRatio(out.left, 24000, 48000);
    };
    // Both still sound; only the noise content differs.
    CHECK(tailHf(1.0f) < tailHf(0.0f) * 0.9);
}

TEST_CASE("clean=1 removes the ensemble humanization but not the choir", "[choir][clean]")
{
    auto render = [](float clean) {
        ChoirParams p;
        p.ensemble = 1.0f;
        p.breath = 0.0f;
        p.clean = clean;
        ChoirEngine engine;
        freshEngine(engine, p);
        engine.reseed(7);
        return run(engine, {noteOn(0, 45, 100), noteOn(0, 52, 100)}, 48000);
    };
    const auto modeled = render(0.0f);
    const auto clinical = render(1.0f);

    // Different sound...
    double diff = 0.0;
    for (size_t i = 0; i < modeled.left.size(); ++i)
        diff += std::abs(double(modeled.left[i]) - clinical.left[i]);
    CHECK(diff > 1.0);

    // ...but `clean` never scales the musical signal: clean=1 must still be
    // a fully usable level, within a few dB of the modeled render.
    CHECK(clinical.rms > 0.01f);
    CHECK(clinical.rms > modeled.rms * 0.7f);
    CHECK(clinical.rms < modeled.rms * 1.4f);
}

TEST_CASE("no ChoirParams default silences the instrument", "[choir][clean]")
{
    // The regression contract from issue #1: a fresh engine with untouched
    // defaults, no CCs and no host assistance must render a usable level.
    ChoirEngine engine;
    freshEngine(engine);
    auto out = run(engine, {noteOn(0, 45, 100), noteOn(0, 52, 100), noteOn(0, 57, 100)},
                   96000);
    const double rmsDb = 20.0 * std::log10(std::max(1.0e-12f, out.rms));
    INFO("default-parameter RMS " << rmsDb << " dBFS");
    CHECK(rmsDb > -45.0);
    CHECK(rmsDb < -6.0);
}
