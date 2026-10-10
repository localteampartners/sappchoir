#pragma once
// Fractional delay-line read position, wrapped into [0, size).
//
// v0.8.2: the cathedral FDN used to compute
//     float pos = float(write) - delay;  while (pos < 0) pos += float(size);
//     int i0 = int(pos);
// A position a hair below zero plus float(size) rounds to EXACTLY size in
// single precision, so i0 == size and the read landed one element past the
// end of the line — a heap over-read. What sits there depends on the heap
// layout of that run: usually 0 (silent), sometimes malloc bookkeeping read as
// a float (~1e13), which hits the FDN as a full-scale impulse. Same bug as
// sappkeys 0.15.1 (its room and comb bank); same fix as the suite's other
// halls: double arithmetic, wrapped both ways, then clamped — so no rounding
// can ever leave the line.
//
// Header-only, allocation-free, realtime-safe.

#include <algorithm>

namespace sapp::choir {

struct DelayTap {
    int i0 = 0;       // first sample, always in [0, size)
    int i1 = 0;       // the one after it, wrapped, always in [0, size)
    float frac = 0;   // weight of i1, in [0, 1]
};

inline DelayTap delayTap(int writePos, float delaySamples, int size) noexcept
{
    DelayTap t;
    if (size <= 0) return t;
    double pos = double(writePos) - double(delaySamples);
    while (pos < 0.0) pos += double(size);
    while (pos >= double(size)) pos -= double(size);
    t.i0 = std::clamp(int(pos), 0, size - 1);
    t.frac = std::clamp(float(pos - double(t.i0)), 0.0f, 1.0f);
    t.i1 = t.i0 + 1 >= size ? 0 : t.i0 + 1;
    return t;
}

} // namespace sapp::choir
