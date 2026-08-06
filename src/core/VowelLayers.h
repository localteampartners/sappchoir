#pragma once
// Vowel morphing for SappChoir.
//
// The morph itself is SappSounds' live CC crossfade engine (SFZ
// xfin_loccN/xfout_hiccN): four vowel layers per note crossfade on the
// Vowel CC while the note sounds. If a library already ships CC-crossfade
// vowel layers on the Vowel CC we use them as-is; otherwise this module
// GENERATES them offline at load — each source sample is refiltered through
// a small biquad formant bank into oo/oh/ah/eh variants, and the four
// copies of every region are registered as crossfading layers.
//
// Deterministic, off the audio thread, no JUCE.

#include <array>
#include <sapp/sounds/InstrumentDefinition.h>

namespace sapp::choir {

// The Vowel CC (SappLink manifest "vowel"). 0 = oo … 127 = eh.
inline constexpr int kVowelCc = 20;

inline constexpr int kNumVowels = 4;
inline constexpr const char* kVowelNames[kNumVowels] = {"oo", "oh", "ah", "eh"};

// Formant targets (choir-register averages, Hz) + per-formant gain (linear).
struct VowelFormants {
    std::array<float, 3> freq;
    std::array<float, 3> gain;
    std::array<float, 3> bandwidth;
};
const VowelFormants& vowelFormants(int vowelIndex);

// Crossfade window of a vowel layer on the Vowel CC 0..127 scale.
struct VowelWindow {
    int inLo = -1, inHi = -1;    // xfin range (-1 = none: full from CC 0)
    int outLo = -1, outHi = -1;  // xfout range (-1 = none: full to CC 127)
};
VowelWindow vowelWindow(int vowelIndex);

// True if the instrument already has live CC crossfades on the Vowel CC
// (library-authored vowel morphing) — generation would be redundant.
bool hasVowelLayers(const sapp::sounds::InstrumentDefinition& definition,
                    int vowelCc = kVowelCc);

// Build the morphable instrument: every attack region is quadrupled, each
// copy's sample refiltered to one vowel and given the layer's crossfade
// window on `vowelCc`. Regions that already have crossfades on `vowelCc`
// pass through untouched. Returns a new immutable instrument.
sapp::sounds::InstrumentPtr makeVowelInstrument(
    const sapp::sounds::InstrumentPtr& base, int vowelCc = kVowelCc);

// Offline formant refilter of one mono channel (exposed for tests):
// 3 parallel RBJ bandpass sections + a small dry blend, RMS-matched to the
// input so vowels keep equal loudness.
void formantFilter(const float* in, float* out, size_t frames,
                   double sampleRate, const VowelFormants& formants);

} // namespace sapp::choir
