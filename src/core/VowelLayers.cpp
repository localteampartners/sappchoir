#include "VowelLayers.h"

#include <cmath>
#include <cstring>

namespace sapp::choir {

using namespace sapp::sounds;

// Choir-register formants. F1/F2 carry the vowel identity; F3 adds presence.
// Gains taper upward; bandwidths widen with frequency (singing voice).
const VowelFormants& vowelFormants(int vowelIndex)
{
    static const VowelFormants table[kNumVowels] = {
        // oo (/u/): dark, closed
        {{310.0f, 870.0f, 2250.0f}, {1.0f, 0.35f, 0.12f}, {70.0f, 90.0f, 140.0f}},
        // oh (/o/): rounded
        {{450.0f, 1030.0f, 2380.0f}, {1.0f, 0.45f, 0.16f}, {80.0f, 100.0f, 140.0f}},
        // ah (/a/): open
        {{730.0f, 1220.0f, 2440.0f}, {1.0f, 0.60f, 0.22f}, {90.0f, 110.0f, 150.0f}},
        // eh (/e/): bright, forward
        {{560.0f, 1850.0f, 2560.0f}, {1.0f, 0.70f, 0.30f}, {80.0f, 110.0f, 150.0f}},
    };
    return table[vowelIndex < 0 ? 0 : vowelIndex >= kNumVowels ? kNumVowels - 1 : vowelIndex];
}

// Equal-power-friendly adjacent windows across the CC range:
//   oo full at 0, oh centred at 42, ah at 85, eh full at 127.
VowelWindow vowelWindow(int vowelIndex)
{
    switch (vowelIndex) {
        case 0:  return {-1, -1, 0, 42};      // oo: fade out 0→42
        case 1:  return {0, 42, 42, 85};      // oh: in 0→42, out 42→85
        case 2:  return {42, 85, 85, 127};    // ah: in 42→85, out 85→127
        default: return {85, 127, -1, -1};    // eh: fade in 85→127
    }
}

bool hasVowelLayers(const InstrumentDefinition& definition, int vowelCc)
{
    for (const auto& region : definition.regions)
        for (const auto& crossfade : region.ccCrossfades)
            if (int(crossfade.cc) == vowelCc)
                return true;
    return false;
}

namespace {

// RBJ constant-0dB-peak bandpass biquad.
struct Biquad {
    double b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1 = 0, z2 = 0;

    void bandpass(double sampleRate, double freq, double bandwidthHz)
    {
        const double w0 = 2.0 * 3.14159265358979 * freq / sampleRate;
        const double q = freq / (bandwidthHz <= 1.0 ? 1.0 : bandwidthHz);
        const double alpha = std::sin(w0) / (2.0 * q);
        const double a0 = 1.0 + alpha;
        b0 = alpha / a0;
        b1 = 0.0;
        b2 = -alpha / a0;
        a1 = -2.0 * std::cos(w0) / a0;
        a2 = (1.0 - alpha) / a0;
        z1 = z2 = 0.0;
    }

    inline float tick(float x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return float(y);
    }
};

double rmsOf(const float* x, size_t frames)
{
    double sum = 0.0;
    for (size_t i = 0; i < frames; ++i) sum += double(x[i]) * x[i];
    return std::sqrt(sum / double(frames == 0 ? 1 : frames));
}

} // namespace

void formantFilter(const float* in, float* out, size_t frames,
                   double sampleRate, const VowelFormants& formants)
{
    if (frames == 0) return;

    Biquad bands[3];
    for (int b = 0; b < 3; ++b) {
        // Keep formants below Nyquist for low-rate sources.
        const double freq = std::min(double(formants.freq[size_t(b)]), sampleRate * 0.45);
        bands[b].bandpass(sampleRate, freq, double(formants.bandwidth[size_t(b)]));
    }

    // Small dry blend keeps the sung source's own life (vibrato, consonant
    // noise) underneath the imposed vowel color.
    constexpr float kDry = 0.22f;
    for (size_t i = 0; i < frames; ++i) {
        const float x = in[i];
        float y = kDry * x;
        for (int b = 0; b < 3; ++b)
            y += formants.gain[size_t(b)] * bands[b].tick(x);
        out[i] = y;
    }

    // RMS-match to the source so all four vowels sit at equal loudness.
    const double inRms = rmsOf(in, frames);
    const double outRms = rmsOf(out, frames);
    if (outRms > 1.0e-9) {
        const float norm = float(inRms / outRms);
        for (size_t i = 0; i < frames; ++i) out[i] *= norm;
    }
}

InstrumentPtr makeVowelInstrument(const InstrumentPtr& base, int vowelCc)
{
    if (!base) return base;
    if (hasVowelLayers(base->definition, vowelCc)) return base;

    auto out = std::make_shared<LoadedInstrument>();
    out->definition = base->definition;
    out->definition.regions.clear();
    out->samples = base->samples;  // originals kept for release/other triggers

    // One refiltered copy of every referenced sample per vowel, built lazily
    // (samples shared between regions are filtered once per vowel).
    std::vector<std::array<SampleIndex, kNumVowels>> vowelCopies(
        base->samples.size(), {kInvalidSample, kInvalidSample, kInvalidSample, kInvalidSample});

    auto vowelSampleFor = [&](SampleIndex source, int vowel) -> SampleIndex {
        if (source < 0 || size_t(source) >= base->samples.size()) return source;
        auto& slot = vowelCopies[size_t(source)][size_t(vowel)];
        if (slot != kInvalidSample) return slot;

        const SampleData& src = base->samples[size_t(source)];
        SampleData copy = src;
        copy.relativePath = src.relativePath + "#" + kVowelNames[vowel];
        for (uint32_t ch = 0; ch < src.channels; ++ch)
            formantFilter(src.data[ch].data(), copy.data[ch].data(),
                          size_t(src.frames), double(src.sampleRate),
                          vowelFormants(vowel));
        out->samples.push_back(std::move(copy));
        slot = SampleIndex(out->samples.size() - 1);
        return slot;
    };

    for (const auto& region : base->definition.regions) {
        // Only attack layers morph; release/legato helper regions pass through.
        const bool morphable = region.trigger == TriggerMode::Attack &&
                               region.sample != kInvalidSample;
        if (!morphable) {
            out->definition.regions.push_back(region);
            continue;
        }
        for (int vowel = 0; vowel < kNumVowels; ++vowel) {
            RegionDefinition layer = region;
            layer.sample = vowelSampleFor(region.sample, vowel);
            layer.samplePath = region.samplePath + "#" + kVowelNames[vowel];
            RegionDefinition::CcCrossfade crossfade;
            crossfade.cc = uint8_t(vowelCc);
            const VowelWindow window = vowelWindow(vowel);
            crossfade.inLo = int16_t(window.inLo);
            crossfade.inHi = int16_t(window.inHi);
            crossfade.outLo = int16_t(window.outLo);
            crossfade.outHi = int16_t(window.outHi);
            layer.ccCrossfades.push_back(crossfade);
            out->definition.regions.push_back(layer);
        }
    }

    out->definition.name = base->definition.name.empty()
                               ? std::string("Vowel Choir")
                               : base->definition.name + " (vowel morph)";
    return out;
}

} // namespace sapp::choir
