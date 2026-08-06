#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

#include "core/CathedralReverb.h"

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
