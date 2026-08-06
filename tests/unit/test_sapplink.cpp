#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "ChoirTestHelpers.h"
#include "core/ChoirRender.h"
#include "core/SappLinkCCMap.h"
#include "core/VowelLayers.h"

// The vendored manifest at tests/data/sapplink-manifest.json mirrors the
// SOURCE OF TRUTH at ~/apps/sapptune/sapplink/manifests/sappchoir.json.
// If sapptune's manifest changes, update the vendored copy AND the table in
// src/core/SappLinkCCMap.cpp together — this test makes silent drift fail CI.

using namespace sapp::choir;
using namespace sapp::choir::sapplink;
using namespace sappchoirtest;

namespace {

struct ManifestRow {
    int cc = -1;
    std::string id, curve;
    float lo = 0, hi = 0;
};

// Minimal extractor for the known manifest shape (no JSON dependency in the
// core test target): parses each object of the "parameters" array.
std::vector<ManifestRow> loadManifest(const std::string& path)
{
    std::ifstream file(path);
    REQUIRE(file.good());
    std::stringstream ss;
    ss << file.rdbuf();
    const std::string text = ss.str();

    auto grabString = [](const std::string& obj, const std::string& key) {
        const auto k = obj.find("\"" + key + "\"");
        if (k == std::string::npos) return std::string();
        const auto q1 = obj.find('"', obj.find(':', k));
        const auto q2 = obj.find('"', q1 + 1);
        return obj.substr(q1 + 1, q2 - q1 - 1);
    };

    std::vector<ManifestRow> rows;
    size_t pos = text.find("\"parameters\"");
    REQUIRE(pos != std::string::npos);
    while ((pos = text.find("{ \"id\"", pos)) != std::string::npos) {
        const auto end = text.find('}', pos);
        const std::string obj = text.substr(pos, end - pos);
        ManifestRow row;
        row.id = grabString(obj, "id");
        row.curve = grabString(obj, "curve");
        row.cc = std::stoi(obj.substr(obj.find(':', obj.find("\"cc\"")) + 1));
        const auto rangeStart = obj.find('[', obj.find("\"range\""));
        const auto comma = obj.find(',', rangeStart);
        row.lo = std::stof(obj.substr(rangeStart + 1, comma - rangeStart - 1));
        row.hi = std::stof(obj.substr(comma + 1, obj.find(']', comma) - comma - 1));
        rows.push_back(row);
        pos = end;
    }
    return rows;
}

} // namespace

TEST_CASE("SappLink table matches the vendored manifest exactly", "[sapplink]")
{
    const auto rows = loadManifest(std::string(SAPPCHOIR_TEST_DATA_DIR) + "/sapplink-manifest.json");
    REQUIRE(rows.size() == size_t(kNumMappings));

    for (const auto& row : rows) {
        INFO("cc " << row.cc << " id " << row.id);
        const auto* mapping = findMapping(row.cc);
        REQUIRE(mapping != nullptr);
        REQUIRE(std::string(mapping->paramId) == row.id);
        REQUIRE(mapping->lo == row.lo);
        REQUIRE(mapping->hi == row.hi);
        REQUIRE(std::string(mapping->curve == Curve::Log ? "log" : "linear") == row.curve);
    }

    // No duplicate CC assignments in the table.
    for (const auto& a : mappings())
        REQUIRE(findMapping(a.cc) == &a);
}

TEST_CASE("reserved controllers stay engine-native", "[sapplink]")
{
    // CC 1 dynamics, CC 11 expression, CC 64 sustain: never in the mapping.
    REQUIRE(findMapping(1) == nullptr);
    REQUIRE(findMapping(11) == nullptr);
    REQUIRE(findMapping(64) == nullptr);
}

TEST_CASE("the Vowel CC is part of the contract", "[sapplink]")
{
    const auto* vowel = findMapping(kVowelCc);
    REQUIRE(vowel != nullptr);
    REQUIRE(std::string(vowel->paramId) == "vowel");
    REQUIRE(std::abs(ccToEngineering(*vowel, 0)) < 1e-6f);
    REQUIRE(std::abs(ccToEngineering(*vowel, 127) - 1.0f) < 1e-6f);
}

TEST_CASE("CC curves interpolate correctly and monotonically", "[sapplink]")
{
    const auto* decay = findMapping(15);  // spaceDecay, log 1..20
    REQUIRE(decay != nullptr);
    REQUIRE(std::abs(ccToEngineering(*decay, 0) - 1.0f) < 1e-4f);
    REQUIRE(std::abs(ccToEngineering(*decay, 127) - 20.0f) < 1e-3f);
    const float mid = ccToEngineering(*decay, 64);  // ≈ geometric mean ~4.5 s
    REQUIRE(mid > 3.5f);
    REQUIRE(mid < 5.5f);

    for (const auto& mapping : mappings()) {
        float previous = ccToEngineering(mapping, 0);
        for (int v = 1; v <= 127; ++v) {
            const float value = ccToEngineering(mapping, v);
            REQUIRE(std::isfinite(value));
            REQUIRE(value >= previous - 1e-6f);
            previous = value;
        }
    }
}

TEST_CASE("applyCcToParams writes the mapped field and ignores others", "[sapplink]")
{
    ChoirParams params;
    REQUIRE(applyCcToParams(params, 20, 127));
    REQUIRE(std::abs(params.vowel - 1.0f) < 1e-5f);
    REQUIRE(applyCcToParams(params, 7, 0));
    REQUIRE(std::abs(params.masterGainDb - (-24.0f)) < 1e-4f);
    REQUIRE_FALSE(applyCcToParams(params, 1, 64));    // dynamics is native
    REQUIRE_FALSE(applyCcToParams(params, 74, 64));   // sappsynth's CC, not ours
}

TEST_CASE("CC 20 in a rendered clip morphs the vowel", "[sapplink]")
{
    auto inst = makeVowelInstrument(makeSawInstrument(), kVowelCc);

    auto renderWithVowelCc = [&](int ccValue) {
        std::vector<sapp::sounds::TimedMidiEvent> song;
        song.push_back({0.0, 0xB0, 0, 20, uint8_t(ccValue), 0});
        song.push_back({0.1, 0x90, 0, 45, 100, 0});
        song.push_back({1.1, 0x80, 0, 45, 0, 0});
        ChoirRenderOptions options;
        options.tailSeconds = 0.3;
        options.params.tailLevel = 0.0f;
        options.params.earlyLevel = 0.0f;
        options.params.breath = 0.0f;
        return renderChoir(inst, song, options);
    };

    auto oo = renderWithVowelCc(0);
    auto eh = renderWithVowelCc(127);
    const double ooRatio = bandEnergy(oo.left, 48000, 1900.0, 8000, 50000) /
                           (bandEnergy(oo.left, 48000, 900.0, 8000, 50000) + 1e-12);
    const double ehRatio = bandEnergy(eh.left, 48000, 1900.0, 8000, 50000) /
                           (bandEnergy(eh.left, 48000, 900.0, 8000, 50000) + 1e-12);
    REQUIRE(ehRatio > ooRatio * 2.0);
}

TEST_CASE("CC 7 in a rendered clip scales output level", "[sapplink]")
{
    auto inst = makeVowelInstrument(makeSawInstrument(), kVowelCc);

    auto renderWithMasterCc = [&](int ccValue) {
        std::vector<sapp::sounds::TimedMidiEvent> song;
        song.push_back({0.0, 0xB0, 0, 7, uint8_t(ccValue), 0});
        song.push_back({0.2, 0x90, 0, 45, 100, 0});
        song.push_back({1.2, 0x80, 0, 45, 0, 0});
        ChoirRenderOptions options;
        options.tailSeconds = 0.3;
        options.params.limiter = false;
        return renderChoir(inst, song, options);
    };

    const float quiet = renderWithMasterCc(0).rms;    // -24 dB
    const float loud = renderWithMasterCc(127).rms;   // +12 dB
    REQUIRE(loud > quiet * 10.0f);
}
