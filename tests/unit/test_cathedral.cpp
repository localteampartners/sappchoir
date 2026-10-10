#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <vector>

#include "core/CathedralReverb.h"
#include "core/DelayRead.h"

using namespace sapp::choir;

namespace {

// Impulse through a processor; returns left response buffer.
template <typename Fn>
std::vector<float> impulseResponse(Fn&& process, int frames)
{
    std::vector<float> inL(size_t(frames), 0.0f), inR(size_t(frames), 0.0f);
    std::vector<float> outL(size_t(frames), 0.0f), outR(size_t(frames), 0.0f);
    inL[0] = inR[0] = 1.0f;
    process(inL.data(), inR.data(), outL.data(), outR.data(), frames);
    return outL;
}

float rmsRange(const std::vector<float>& x, size_t a, size_t b)
{
    double sum = 0.0;
    for (size_t i = a; i < b && i < x.size(); ++i) sum += double(x[i]) * x[i];
    return float(std::sqrt(sum / double(b - a)));
}

double hfRatio(const std::vector<float>& x, size_t a, size_t b)
{
    double hf = 0.0, total = 0.0;
    float prev = 0.0f;
    for (size_t i = a; i < b && i < x.size(); ++i) {
        const float d = x[i] - prev;
        hf += double(d) * d;
        total += double(x[i]) * x[i];
        prev = x[i];
    }
    return total > 0.0 ? hf / total : 0.0;
}

} // namespace

TEST_CASE("early reflections arrive after predelay and decay", "[cathedral]")
{
    EarlyReflections er;
    er.prepare(48000);
    er.setSpace(25.0f, 0.4f);  // 25 ms predelay

    auto ir = impulseResponse(
        [&](const float* a, const float* b, float* c, float* d, int n) { er.process(a, b, c, d, n); },
        24000);

    // Nothing before the predelay (~1200 frames), energy after.
    float before = 0.0f, after = 0.0f;
    for (size_t i = 0; i < 1100; ++i) before = std::max(before, std::abs(ir[i]));
    for (size_t i = 1200; i < 9000; ++i) after = std::max(after, std::abs(ir[i]));
    CHECK(before < 1.0e-6f);
    CHECK(after > 0.01f);
}

TEST_CASE("cathedral tail is dense, decaying, and finite", "[cathedral]")
{
    CathedralReverb cathedral;
    cathedral.prepare(48000);
    cathedral.setParams(1.15f, 4.0f, 0.5f, 0.3f);

    auto ir = impulseResponse(
        [&](const float* a, const float* b, float* c, float* d, int n) { cathedral.process(a, b, c, d, n); },
        192000);

    for (float v : ir) REQUIRE(std::isfinite(v));

    const float early = rmsRange(ir, 4800, 24000);      // 0.1–0.5 s
    const float mid = rmsRange(ir, 96000, 115200);      // 2.0–2.4 s
    const float late = rmsRange(ir, 172800, 192000);    // 3.6–4.0 s
    CHECK(early > 1.0e-4f);   // tail exists
    CHECK(mid < early);       // it decays
    CHECK(late < mid);        // monotonically-ish
    CHECK(late > 1.0e-8f);    // but is still alive inside T60
}

TEST_CASE("longer decay setting yields longer tail", "[cathedral]")
{
    auto tailAt = [](float decaySeconds) {
        CathedralReverb cathedral;
        cathedral.prepare(48000);
        cathedral.setParams(1.15f, decaySeconds, 0.4f, 0.3f);
        auto ir = impulseResponse(
            [&](const float* a, const float* b, float* c, float* d, int n) { cathedral.process(a, b, c, d, n); },
            192000);
        return rmsRange(ir, 144000, 192000);  // energy at 3–4 s
    };
    CHECK(tailAt(12.0f) > tailAt(2.0f) * 3.0f);
}

TEST_CASE("sacred-space decay reaches 20 seconds", "[cathedral]")
{
    CathedralReverb cathedral;
    cathedral.prepare(48000);
    cathedral.setParams(1.5f, 20.0f, 0.2f, 0.3f);
    auto ir = impulseResponse(
        [&](const float* a, const float* b, float* c, float* d, int n) { cathedral.process(a, b, c, d, n); },
        480000);  // 10 s
    // With T60 = 20 s the tail must clearly still ring at 9–10 s.
    CHECK(rmsRange(ir, 432000, 480000) > 1.0e-5f);
    for (float v : ir) REQUIRE(std::isfinite(v));
}

TEST_CASE("damping darkens the tail", "[cathedral]")
{
    auto tailHf = [](float damping) {
        CathedralReverb cathedral;
        cathedral.prepare(48000);
        cathedral.setParams(1.15f, 6.0f, damping, 0.3f);
        auto ir = impulseResponse(
            [&](const float* a, const float* b, float* c, float* d, int n) { cathedral.process(a, b, c, d, n); },
            96000);
        return hfRatio(ir, 24000, 96000);
    };
    CHECK(tailHf(0.0f) > tailHf(0.9f) * 1.5);
}

// v0.8.2 — the cathedral read one sample past its line.
// The FDN wrapped its modulated fractional read in single precision, so a
// position a hair below zero rounded up to exactly `size` and the read landed
// one element past the line (heap over-read). Same bug as sappkeys 0.15.1.
// With the old read, a 2-minute run at Cathedral Size 0.75 / 48 kHz goes past
// the line in 2 blocks, at 0.2 in 12; at 96 kHz in 41 and 84.

TEST_CASE("delay taps never leave the line, even when float rounding would", "[cathedral][v0.8.2]")
{
    // Line 0 at 48 kHz is 4298 floats; the write head at 643 with a delay one
    // ulp above it puts the old read position at -6.1e-5.
    const int size = 4298, write = 643;
    const float delay = std::nextafter(643.0f, 1.0e9f);   // 643.000061

    // What the old code computed: the wrap lands ON size.
    float old = float(write) - delay;
    while (old < 0.0f) old += float(size);
    REQUIRE(int(old) == size);

    const DelayTap tap = delayTap(write, delay, size);
    REQUIRE(tap.i0 >= 0);
    REQUIRE(tap.i0 < size);
    REQUIRE(tap.i1 >= 0);
    REQUIRE(tap.i1 < size);
    REQUIRE(tap.frac >= 0.0f);
    REQUIRE(tap.frac <= 1.0f);
    // The read just behind the write head, nearly all on the newest sample
    // before it — not a jump to the far end of the line.
    REQUIRE(tap.i0 == size - 1);
    REQUIRE(tap.i1 == 0);

    // Every write position against delays within a few ulps of it, across
    // the cathedral's real line lengths (44.1 / 48 / 96 kHz, shortest and
    // longest line) plus a tiny line.
    int outside = 0;
    for (const int n : {64, 3954, 10959, 4298, 11922, 8533, 23781}) {
        for (int w = 0; w < n; ++w) {
            for (const float d0 : {float(w), float(w) + 1.0f, float(n - 1), float(n - 8)}) {
                float d = d0;
                for (int step = 0; step < 6; ++step) {
                    const DelayTap t = delayTap(w, d, n);
                    if (t.i0 < 0 || t.i0 >= n || t.i1 < 0 || t.i1 >= n ||
                        !(t.frac >= 0.0f && t.frac <= 1.0f))
                        ++outside;
                    d = std::nextafter(d, 1.0e9f);
                }
            }
        }
    }
    REQUIRE(outside == 0);

    // Out-of-range delays (negative, or longer than the line) still wrap in.
    for (const float d : {-0.5f, -1.0e-7f, float(size), float(size) + 0.25f, 3.0f * float(size)}) {
        const DelayTap t = delayTap(write, d, size);
        REQUIRE(t.i0 >= 0);
        REQUIRE(t.i0 < size);
        REQUIRE(t.i1 >= 0);
        REQUIRE(t.i1 < size);
    }
}

TEST_CASE("the cathedral runs two minutes without an over-read or a blow-up", "[cathedral][v0.8.2]")
{
    // The sizes and rates where the old read went past the line (see above).
    // Under AddressSanitizer the old code fails this with a heap over-read;
    // in a plain build the bound below is the audible half of the contract.
    for (const double sr : {48000.0, 96000.0}) {
        for (const float roomSize : {0.2f, 0.75f}) {
            CathedralReverb cathedral;
            cathedral.prepare(sr);
            cathedral.setParams(roomSize, 6.5f, 0.55f, 0.3f);

            constexpr int kBlock = 512;
            std::vector<float> inL(kBlock), inR(kBlock), oL(kBlock), oR(kBlock);
            uint32_t rng = 12345u;
            float peak = 0.0f;
            bool finite = true;
            const int blocks = int(120.0 * sr / kBlock);
            for (int b = 0; b < blocks; ++b) {
                for (int f = 0; f < kBlock; ++f) {
                    rng = rng * 1664525u + 1013904223u;
                    const float noise = (float(rng >> 8) / 16777216.0f - 0.5f) * 0.2f;
                    inL[size_t(f)] = noise;
                    inR[size_t(f)] = -noise;
                }
                cathedral.process(inL.data(), inR.data(), oL.data(), oR.data(), kBlock);
                for (int f = 0; f < kBlock; ++f) {
                    finite = finite && std::isfinite(oL[size_t(f)]) && std::isfinite(oR[size_t(f)]);
                    peak = std::max({peak, std::abs(oL[size_t(f)]), std::abs(oR[size_t(f)])});
                }
            }
            INFO("sr " << sr << " size " << roomSize << " peak " << peak);
            REQUIRE(finite);
            // Noise at +-0.1 into a lossy FDN: the tail stays under full scale.
            REQUIRE(peak < 1.0f);
        }
    }
}
