#pragma once
// SappChoir room: distant early reflections + long cathedral tail
// (8-line FDN, Householder feedback, heavy absorption options, gentle
// modulation). Adapted from SappOrchestra's hall but tuned for sacred
// space: longer base delays, decay up to 20 s, stronger damping range,
// slower modulation. Framework-independent, realtime-safe after prepare().

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace sapp::choir {

// ------------------------------------------------------------ early taps ---
// A choir sits far away in the apse: taps are later and darker than a
// concert-hall early pattern, with a fixed gentle left/right spread.
class EarlyReflections {
public:
    void prepare(double sampleRate)
    {
        sampleRate_ = sampleRate;
        const double maxSeconds = 0.35;
        buffer_.assign(size_t(sampleRate * maxSeconds) + 4, 0.0f);
        writePos_ = 0;
        lpL_ = lpR_ = 0.0f;
    }

    // predelayMs: distance to the first stone reflection. damping 0..1.
    void setSpace(float predelayMs, float damping)
    {
        predelaySamples_ = float(predelayMs * 0.001 * sampleRate_);
        dampCoef_ = 1.0f - std::clamp(damping, 0.0f, 0.98f);
    }

    void process(const float* inL, const float* inR, float* outL, float* outR, int frames)
    {
        static constexpr float tapMs[8] = {19.3f, 27.7f, 36.1f, 47.9f, 59.3f, 71.9f, 84.1f, 96.7f};
        static constexpr float tapGain[8] = {0.78f, 0.66f, 0.57f, 0.47f, 0.39f, 0.32f, 0.26f, 0.21f};

        const int size = int(buffer_.size());
        for (int f = 0; f < frames; ++f) {
            buffer_[size_t(writePos_)] = 0.5f * (inL[f] + inR[f]);

            float l = 0.0f, r = 0.0f;
            for (int t = 0; t < 8; ++t) {
                const float delay = predelaySamples_ + float(tapMs[t] * 0.001 * sampleRate_);
                int idx = writePos_ - int(delay);
                while (idx < 0) idx += size;
                const float v = buffer_[size_t(idx)] * tapGain[t];
                if (t % 2 == 0) { l += v; r += v * 0.55f; }
                else            { r += v; l += v * 0.55f; }
            }
            // Stone absorbs highs on every bounce; distance darkens further.
            lpL_ += dampCoef_ * (l - lpL_);
            lpR_ += dampCoef_ * (r - lpR_);
            outL[f] = lpL_ * 0.5f;
            outR[f] = lpR_ * 0.5f;

            if (++writePos_ >= size) writePos_ = 0;
        }
    }

private:
    std::vector<float> buffer_;
    int writePos_ = 0;
    double sampleRate_ = 48000.0;
    float predelaySamples_ = 0.0f;
    float dampCoef_ = 0.5f;
    float lpL_ = 0.0f, lpR_ = 0.0f;
};

// -------------------------------------------------------- cathedral FDN ----
class CathedralReverb {
public:
    void prepare(double sampleRate)
    {
        sampleRate_ = sampleRate;
        // Mutually prime base delays (ms), ~1.35x a concert hall: a nave is
        // long, and the first echoes come back slow and dense.
        static constexpr float baseMs[kLines] = {40.1f, 50.3f, 55.7f, 63.9f,
                                                 72.9f, 83.3f, 97.1f, 112.3f};
        for (int i = 0; i < kLines; ++i) {
            baseSamples_[i] = float(baseMs[size_t(i)] * 0.001 * sampleRate);
            const size_t cap = size_t(baseSamples_[size_t(i)] * 2.2f) + 64;
            lines_[size_t(i)].assign(cap, 0.0f);
            writePos_[i] = 0;
            damp_[i] = 0.0f;
            lfoPhase_[i] = float(i) * 0.785f;
        }
        inDiffL_.prepare(sampleRate, 5.9f, 0.64f);
        inDiffR_.prepare(sampleRate, 4.3f, 0.64f);
        update();
    }

    // size 0.2..1.5, decay 0.5..20 s (T60), damping 0..1 (stone → curtains),
    // modDepth 0..1 (kept subtle — a cathedral does not chorus).
    void setParams(float size, float decaySeconds, float damping, float modDepth)
    {
        size_ = std::clamp(size, 0.2f, 1.5f);
        decay_ = std::clamp(decaySeconds, 0.5f, 20.0f);
        damping_ = std::clamp(damping, 0.0f, 1.0f);
        modDepth_ = std::clamp(modDepth, 0.0f, 1.0f);
        update();
    }

    void process(const float* inL, const float* inR, float* outL, float* outR, int frames)
    {
        for (int f = 0; f < frames; ++f) {
            const float inMono = 0.35f * (inDiffL_.tick(inL[f]) + inDiffR_.tick(inR[f]));

            // Read modulated taps.
            float read[kLines];
            float sum = 0.0f;
            for (int i = 0; i < kLines; ++i) {
                lfoPhase_[i] += lfoInc_[i];
                if (lfoPhase_[i] > 6.2831853f) lfoPhase_[i] -= 6.2831853f;
                const float mod = std::sin(lfoPhase_[i]) * modSamples_;
                const float delay = delaySamples_[i] + mod;
                const int size = int(lines_[size_t(i)].size());
                float pos = float(writePos_[i]) - delay;
                while (pos < 0.0f) pos += float(size);
                const int i0 = int(pos);
                const float frac = pos - float(i0);
                const int i1 = i0 + 1 >= size ? 0 : i0 + 1;
                read[i] = lines_[size_t(i)][size_t(i0)] * (1.0f - frac) + lines_[size_t(i)][size_t(i1)] * frac;
                sum += read[i];
            }

            // Householder feedback: y_i = g * (read_i - (2/N) * sum) + input.
            const float k = 2.0f / float(kLines);
            for (int i = 0; i < kLines; ++i) {
                float v = feedback_[i] * (read[i] - k * sum) + inMono;
                // One-pole damping inside the loop.
                damp_[i] += dampCoef_ * (v - damp_[i]);
                v = damp_[i];
                lines_[size_t(i)][size_t(writePos_[i])] = v;
                if (++writePos_[i] >= int(lines_[size_t(i)].size())) writePos_[i] = 0;
            }

            outL[f] = (read[0] - read[2] + read[4] - read[6]) * 0.35f;
            outR[f] = (read[1] - read[3] + read[5] - read[7]) * 0.35f;
        }
    }

private:
    static constexpr int kLines = 8;

    struct Allpass {
        void prepare(double sampleRate, float ms, float g)
        {
            buffer.assign(size_t(ms * 0.001 * sampleRate) + 2, 0.0f);
            pos = 0;
            gain = g;
        }
        float tick(float x)
        {
            const float d = buffer[size_t(pos)];
            const float y = -gain * x + d;
            buffer[size_t(pos)] = x + gain * y;
            if (++pos >= int(buffer.size())) pos = 0;
            return y;
        }
        std::vector<float> buffer;
        int pos = 0;
        float gain = 0.6f;
    };

    void update()
    {
        for (int i = 0; i < kLines; ++i) {
            delaySamples_[i] = std::min(baseSamples_[i] * size_ * 2.0f,
                                        float(lines_[size_t(i)].size()) - 8.0f);
            // Per-line gain for a uniform T60 across all line lengths.
            feedback_[i] = std::pow(10.0f, -3.0f * delaySamples_[i] /
                                                (decay_ * float(sampleRate_)));
            // Slower LFOs than a hall — long tails must not warble.
            lfoInc_[i] = float((0.17 + 0.09 * i) * 2.0 * 3.14159265 / sampleRate_);
        }
        // Wider absorption range than a hall: 1 = heavy drapes/bodies.
        dampCoef_ = 1.0f - 0.92f * damping_;
        modSamples_ = 0.8f + modDepth_ * 5.0f;
    }

    double sampleRate_ = 48000.0;
    std::array<std::vector<float>, kLines> lines_;
    float baseSamples_[kLines] = {};
    float delaySamples_[kLines] = {};
    float feedback_[kLines] = {};
    float damp_[kLines] = {};
    float lfoPhase_[kLines] = {};
    float lfoInc_[kLines] = {};
    int writePos_[kLines] = {};
    Allpass inDiffL_, inDiffR_;

    float size_ = 1.15f, decay_ = 6.5f, damping_ = 0.55f, modDepth_ = 0.3f;
    float dampCoef_ = 0.49f, modSamples_ = 2.3f;
};

} // namespace sapp::choir
