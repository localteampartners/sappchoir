#pragma once
// Deterministic offline render through the full choir chain
// (sampler + vowel crossfades → dynamics/expression → breath/ensemble →
// early reflections → cathedral tail).

#include <cstdint>
#include <vector>

#include <sapp/sounds/MidiFile.h>

#include "ChoirEngine.h"

namespace sapp::choir {

struct ChoirRenderOptions {
    double sampleRate = 48000.0;
    int blockFrames = 512;
    double tailSeconds = 8.0;    // a cathedral rings long
    uint32_t seed = 0x5A9C401Au;
    ChoirParams params;
};

struct ChoirRenderOutput {
    std::vector<float> left, right;
    double sampleRate = 48000.0;
    float peak = 0.0f;
    float rms = 0.0f;
};

ChoirRenderOutput renderChoir(const sapp::sounds::InstrumentPtr& instrument,
                              const std::vector<sapp::sounds::TimedMidiEvent>& events,
                              const ChoirRenderOptions& options,
                              int articulationIndex = -1);

} // namespace sapp::choir
