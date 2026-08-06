#pragma once
// Shared helpers for SappChoir unit tests: generated instruments, block
// rendering, and simple spectral measurements.

#include <cmath>
#include <memory>
#include <vector>

#include <sapp/sounds/InstrumentDefinition.h>
#include <sapp/sounds/PlaybackEngine.h>

#include "core/ChoirEngine.h"

namespace sappchoirtest {

using sapp::sounds::LoadedInstrument;
using sapp::sounds::MidiEvent;
using sapp::sounds::RegionDefinition;
using sapp::sounds::SampleData;

inline MidiEvent noteOn(uint32_t frame, uint8_t note, uint8_t vel)
{
    MidiEvent e;
    e.type = MidiEvent::Type::NoteOn;
    e.frame = frame;
    e.note = note;
    e.value = vel;
    return e;
}

inline MidiEvent noteOff(uint32_t frame, uint8_t note)
{
    MidiEvent e;
    e.type = MidiEvent::Type::NoteOff;
    e.frame = frame;
    e.note = note;
    return e;
}

inline MidiEvent controller(uint32_t frame, uint8_t cc, uint8_t value)
{
    MidiEvent e;
    e.type = MidiEvent::Type::Controller;
    e.frame = frame;
    e.note = cc;
    e.value = value;
    return e;
}

// Band-limited sawtooth sample (harmonic-rich, loops on whole periods).
// 100 Hz at 48 kHz -> 480-sample period.
inline SampleData makeSawSample(double freq = 100.0, uint32_t rate = 48000,
                                double seconds = 1.0)
{
    SampleData s;
    s.relativePath = "gen-saw";
    s.sampleRate = rate;
    s.channels = 1;
    s.frames = uint64_t(seconds * rate);
    s.data.assign(1, std::vector<float>(size_t(s.frames), 0.0f));
    const int harmonics = int(double(rate) * 0.45 / freq);
    for (uint64_t i = 0; i < s.frames; ++i) {
        double v = 0.0;
        for (int h = 1; h <= harmonics; ++h)
            v += std::sin(2.0 * 3.14159265358979 * freq * double(h) * double(i) / rate) / double(h);
        s.data[0][size_t(i)] = float(v * 0.4);
    }
    return s;
}

// One-region looped saw instrument covering the full key range, root A2 (45).
inline std::shared_ptr<LoadedInstrument> makeSawInstrument()
{
    auto inst = std::make_shared<LoadedInstrument>();
    inst->samples.push_back(makeSawSample());
    RegionDefinition r;
    r.sample = 0;
    r.samplePath = "gen-saw";
    r.loKey = 0; r.hiKey = 127; r.rootKey = 45;
    r.ampeg.release = 0.05f;
    r.loop.mode = sapp::sounds::LoopMode::Continuous;
    r.loop.explicitMode = true;
    r.loop.start = 4800;          // whole 480-sample periods
    r.loop.end = 43199;
    inst->definition.regions.push_back(r);
    inst->definition.name = "Test Saw";
    return inst;
}

struct Rendered {
    std::vector<float> left, right;
    float peak = 0.0f;
    float rms = 0.0f;
};

inline Rendered run(sapp::choir::ChoirEngine& engine, std::vector<MidiEvent> events,
                    int totalFrames, int block = 512)
{
    Rendered out;
    out.left.assign(size_t(totalFrames), 0.0f);
    out.right.assign(size_t(totalFrames), 0.0f);
    size_t next = 0;
    for (int start = 0; start < totalFrames; start += block) {
        const int frames = std::min(block, totalFrames - start);
        std::vector<MidiEvent> blockEvents;
        while (next < events.size() && events[next].frame < uint32_t(start + frames)) {
            MidiEvent e = events[next];
            e.frame = e.frame >= uint32_t(start) ? e.frame - uint32_t(start) : 0;
            blockEvents.push_back(e);
            ++next;
        }
        engine.process(blockEvents.data(), int(blockEvents.size()),
                       out.left.data() + start, out.right.data() + start, frames);
    }
    double sumSq = 0.0;
    for (size_t i = 0; i < out.left.size(); ++i) {
        out.peak = std::max({out.peak, std::abs(out.left[i]), std::abs(out.right[i])});
        sumSq += double(out.left[i]) * out.left[i] + double(out.right[i]) * out.right[i];
    }
    out.rms = float(std::sqrt(sumSq / double(out.left.size() * 2)));
    return out;
}

// Goertzel single-bin energy over [start, end).
inline double bandEnergy(const std::vector<float>& x, double sampleRate,
                         double freq, size_t start, size_t end)
{
    if (end > x.size()) end = x.size();
    if (start >= end) return 0.0;
    const double w = 2.0 * 3.14159265358979 * freq / sampleRate;
    const double coeff = 2.0 * std::cos(w);
    double s0 = 0.0, s1 = 0.0, s2 = 0.0;
    for (size_t i = start; i < end; ++i) {
        s0 = double(x[i]) + coeff * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    const double n = double(end - start);
    return (s1 * s1 + s2 * s2 - coeff * s1 * s2) / (n * n);
}

// First-difference HF energy ratio (crude high-pass measure).
inline double hfRatio(const std::vector<float>& x, size_t start = 0, size_t end = 0)
{
    if (end == 0 || end > x.size()) end = x.size();
    double hf = 0.0, total = 0.0;
    float prev = 0.0f;
    for (size_t i = start; i < end; ++i) {
        const float d = x[i] - prev;
        hf += double(d) * d;
        total += double(x[i]) * x[i];
        prev = x[i];
    }
    return total > 0.0 ? hf / total : 0.0;
}

} // namespace sappchoirtest
