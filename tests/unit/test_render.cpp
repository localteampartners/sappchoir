#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "ChoirTestHelpers.h"
#include "core/ChoirRender.h"
#include "core/VowelLayers.h"

using namespace sapp::choir;
using namespace sapp::sounds;
using namespace sappchoirtest;

namespace {

std::vector<TimedMidiEvent> simplePhrase()
{
    std::vector<TimedMidiEvent> song;
    song.push_back({0.1, 0x90, 0, 45, 100, 0});
    song.push_back({0.1, 0x90, 0, 52, 96, 0});
    song.push_back({1.4, 0x80, 0, 45, 0, 0});
    song.push_back({1.4, 0x80, 0, 52, 0, 0});
    return song;
}

} // namespace

TEST_CASE("offline render is deterministic under a fixed seed", "[render]")
{
    auto inst = makeVowelInstrument(makeSawInstrument(), kVowelCc);
    ChoirRenderOptions options;
    options.tailSeconds = 1.0;
    options.params.ensemble = 0.8f;
    options.params.breath = 0.5f;

    auto a = renderChoir(inst, simplePhrase(), options);
    auto b = renderChoir(inst, simplePhrase(), options);
    REQUIRE(a.left.size() == b.left.size());
    REQUIRE(a.left == b.left);
    REQUIRE(a.right == b.right);

    options.seed = 12345;
    auto c = renderChoir(inst, simplePhrase(), options);
    double diff = 0.0;
    for (size_t i = 0; i < a.left.size() && i < c.left.size(); ++i)
        diff += std::abs(double(a.left[i]) - c.left[i]);
    CHECK(diff > 1.0e-2);  // new seed, new ensemble take
}

TEST_CASE("render produces audio and a long sacred tail", "[render]")
{
    auto inst = makeVowelInstrument(makeSawInstrument(), kVowelCc);
    ChoirRenderOptions options;
    options.tailSeconds = 4.0;
    options.params.spaceDecay = 8.0f;

    auto out = renderChoir(inst, simplePhrase(), options);
    REQUIRE(!out.left.empty());
    CHECK(out.peak > 0.02f);
    CHECK(out.peak <= 1.0f);

    // Notes end at 1.4 s; the space still rings at 3 s.
    const size_t at3s = size_t(3.0 * options.sampleRate);
    double lateSum = 0.0;
    for (size_t i = at3s; i < at3s + 24000 && i < out.left.size(); ++i)
        lateSum += std::abs(out.left[i]);
    CHECK(lateSum / 24000.0 > 1.0e-5);
}
