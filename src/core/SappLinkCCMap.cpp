#include "SappLinkCCMap.h"

#include <algorithm>
#include <cmath>

namespace sapp::choir::sapplink {

// CC assignment follows the SappLink conventions (PROTOCOL.md): standard MMA
// CCs where one exists (7 volume, 91 reverb send), free CCs 14–31 otherwise.
// Ranges are the plugin's real APVTS ranges — the manifest mirrors these.
const std::array<CCMapping, kNumMappings>& mappings()
{
    static const std::array<CCMapping, kNumMappings> table { {
        // CC 3 is the suite-wide `clean` convention (sappsynth / sappkeys /
        // sapporchestra / sappkit all use it): 0 = fully modeled, 1 = clinical.
        { 3,  "clean",        &ChoirParams::clean,        0.0f,   1.0f,  Curve::Linear },
        { 7,  "masterGain",   &ChoirParams::masterGainDb, -24.0f, 12.0f, Curve::Linear },
        { 14, "earlyLevel",   &ChoirParams::earlyLevel,   0.0f,   1.0f,  Curve::Linear },
        { 15, "spaceDecay",   &ChoirParams::spaceDecay,   1.0f,   20.0f, Curve::Log },
        { 18, "width",        &ChoirParams::width,        0.0f,   2.0f,  Curve::Linear },
        { 19, "spaceDamping", &ChoirParams::spaceDamping, 0.0f,   1.0f,  Curve::Linear },
        { 20, "vowel",        &ChoirParams::vowel,        0.0f,   1.0f,  Curve::Linear },
        { 21, "breath",       &ChoirParams::breath,       0.0f,   1.0f,  Curve::Linear },
        { 22, "ensemble",     &ChoirParams::ensemble,     0.0f,   1.0f,  Curve::Linear },
        { 68, "legato",       &ChoirParams::legato,       0.0f,   1.0f,  Curve::Linear },
        { 91, "tailLevel",    &ChoirParams::tailLevel,    0.0f,   1.0f,  Curve::Linear },
        { 92, "spaceSize",    &ChoirParams::spaceSize,    0.2f,   1.5f,  Curve::Linear },
    } };
    return table;
}

const CCMapping* findMapping(int cc)
{
    for (const auto& mapping : mappings())
        if (mapping.cc == cc)
            return &mapping;
    return nullptr;
}

float ccToEngineering(const CCMapping& mapping, int ccValue)
{
    const float t = float(std::clamp(ccValue, 0, 127)) / 127.0f;
    if (mapping.curve == Curve::Log)
        return mapping.lo * std::pow(mapping.hi / mapping.lo, t);
    return mapping.lo + (mapping.hi - mapping.lo) * t;
}

bool applyCcToParams(ChoirParams& params, int cc, int ccValue)
{
    const auto* mapping = findMapping(cc);
    if (mapping == nullptr)
        return false;
    params.*(mapping->field) = ccToEngineering(*mapping, ccValue);
    return true;
}

} // namespace sapp::choir::sapplink
