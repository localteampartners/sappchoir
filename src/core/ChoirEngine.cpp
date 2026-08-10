#include "ChoirEngine.h"

#include <algorithm>
#include <cmath>

namespace sapp::choir {

using sapp::sounds::MidiEvent;

namespace {
inline float dbToGain(float db) noexcept { return std::pow(10.0f, db * 0.05f); }

// One-pole smoothing coefficient for ~t milliseconds.
inline float smoothCoef(double sampleRate, float ms) noexcept
{
    return 1.0f - std::exp(-1.0f / (float(sampleRate) * ms * 0.001f));
}
} // namespace

ChoirEngine::ChoirEngine() = default;

void ChoirEngine::prepare(double sampleRate, int maxBlockFrames)
{
    sampleRate_ = sampleRate;
    maxBlock_ = maxBlockFrames;
    sampler_.prepare(sampleRate, maxBlockFrames);
    early_.prepare(sampleRate);
    cathedral_.prepare(sampleRate);

    const size_t n = size_t(maxBlockFrames);
    dryL_.assign(n, 0.0f); dryR_.assign(n, 0.0f);
    sendL_.assign(n, 0.0f); sendR_.assign(n, 0.0f);
    earlyL_.assign(n, 0.0f); earlyR_.assign(n, 0.0f);
    tailL_.assign(n, 0.0f); tailR_.assign(n, 0.0f);

    lpL_ = lpR_ = airLpL_ = airLpR_ = noiseLp_ = envFollow_ = 0.0f;
    smNoise_ = 0.0f;
    liveDynamics_ = liveExpression_ = -1.0f;
    lastSentVowelCc_ = -1;
    lastSize_ = lastDecay_ = lastDamp_ = lastPredelay_ = -1.0f;
    lastQuality_ = -1;
    lastEnsembleCents_ = -1.0f;
    lastLegato_ = -1;
}

void ChoirEngine::setInstrument(sapp::sounds::InstrumentPtr instrument)
{
    sampler_.setInstrument(std::move(instrument));
}
void ChoirEngine::collectRetired() { sampler_.collectRetired(); }
sapp::sounds::InstrumentPtr ChoirEngine::currentInstrument() const
{
    return sampler_.currentInstrument();
}

void ChoirEngine::selectArticulation(int index)
{
    auto inst = sampler_.currentInstrument();
    if (!inst) return;
    const auto& arts = inst->definition.articulations;
    if (index < 0 || size_t(index) >= arts.size()) return;
    if (arts[size_t(index)].keyswitch < 0) return;
    pendingArticulationKeyswitch_.store(arts[size_t(index)].keyswitch,
                                        std::memory_order_release);
}

void ChoirEngine::setParams(const ChoirParams& params)
{
    const int inactive = 1 - paramIndex_.load(std::memory_order_acquire);
    paramSlots_[inactive] = params;
    paramIndex_.store(inactive, std::memory_order_release);
}

ChoirParams ChoirEngine::params() const
{
    return paramSlots_[paramIndex_.load(std::memory_order_acquire)];
}

void ChoirEngine::resetSequences() { sampler_.resetSequences(); }
void ChoirEngine::reseed(uint32_t seed)
{
    sampler_.reseed(seed);
    noiseState_ = seed * 2654435761u + 0x9E3779B9u;
}

void ChoirEngine::applyQuality(const ChoirParams& p) noexcept
{
    if (p.quality != lastQuality_) {
        lastQuality_ = p.quality;
        sampler_.setInterpolationQuality(p.quality == 0 ? 0 : 1);
    }
    // Ensemble size: per-note random detune widens the section. A large
    // choir spreads to ~±7 cents per singer. `clean` scales the detune away
    // (it is humanization, not size — see cleanScale()).
    const float cents = p.ensemble * 14.0f * cleanScale(p);
    if (cents != lastEnsembleCents_) {
        lastEnsembleCents_ = cents;
        sampler_.setRandomTuneCents(cents);
    }
    const int legato = p.legato >= 0.5f ? 1 : 0;
    if (legato != lastLegato_) {
        lastLegato_ = legato;
        // Voices slur slowly: longer skip and fade than an orchestral line.
        sampler_.setLegato(legato != 0, 0.08f, 0.06f);
    }
}

void ChoirEngine::process(const MidiEvent* events, int eventCount,
                          float* outL, float* outR, int frames) noexcept
{
    const ChoirParams p = paramSlots_[paramIndex_.load(std::memory_order_acquire)];
    applyQuality(p);

    // Live CC following: CC1/CC11 persistently override the parameter values
    // so a ridden phrase shapes the performance. The Vowel CC is tracked so
    // parameter-driven injection stays in sync with incoming CC.
    for (int i = 0; i < eventCount; ++i) {
        if (events[i].type == MidiEvent::Type::Controller) {
            if (events[i].note == 1) liveDynamics_ = float(events[i].value) / 127.0f;
            else if (events[i].note == 11) liveExpression_ = float(events[i].value) / 127.0f;
            else if (events[i].note == kVowelCc) lastSentVowelCc_ = events[i].value;
        }
    }
    const float dynamics = liveDynamics_ >= 0.0f ? liveDynamics_ : p.dynamics;
    const float expression = liveExpression_ >= 0.0f ? liveExpression_ : p.expression;

    // Injected events ahead of this block: vowel parameter → Vowel CC (the
    // sampler's crossfade engine is the morph), UI articulation keyswitch.
    MidiEvent localEvents[259];
    int localCount = 0;

    const int vowelCc = int(std::clamp(p.vowel, 0.0f, 1.0f) * 127.0f + 0.5f);
    if (vowelCc != lastSentVowelCc_) {
        lastSentVowelCc_ = vowelCc;
        MidiEvent e;
        e.type = MidiEvent::Type::Controller;
        e.frame = 0;
        e.note = uint8_t(kVowelCc);
        e.value = uint8_t(vowelCc);
        localEvents[localCount++] = e;
    }

    const int ks = pendingArticulationKeyswitch_.exchange(-1, std::memory_order_acq_rel);
    if (ks >= 0) {
        MidiEvent e;
        e.type = MidiEvent::Type::NoteOn;
        e.frame = 0;
        e.note = uint8_t(ks);
        e.value = 1;
        localEvents[localCount++] = e;
        MidiEvent off = e;
        off.type = MidiEvent::Type::NoteOff;
        localEvents[localCount++] = off;
    }
    for (int i = 0; i < eventCount && localCount < 259; ++i)
        localEvents[localCount++] = events[i];

    // --- dry sampler render -------------------------------------------------
    const int n = std::min(frames, maxBlock_);
    std::fill(dryL_.begin(), dryL_.begin() + n, 0.0f);
    std::fill(dryR_.begin(), dryR_.begin() + n, 0.0f);
    sampler_.process(localEvents, localCount, dryL_.data(), dryR_.data(), n);

    // --- target gains -------------------------------------------------------
    // CC1 dynamics: level + timbre. A choir's pp is hushed (-18 dB) and dark.
    const float dynGainTarget = dbToGain(-18.0f * (1.0f - dynamics));
    const float exprGainTarget = dbToGain(-40.0f * (1.0f - expression));
    // Timbre: LP cutoff from ~900 Hz (pp) to ~14 kHz (ff).
    const float bright = 0.15f + 0.85f * dynamics;
    const float cutoffHz = 900.0f * std::pow(14000.0f / 900.0f, std::pow(bright, 0.8f));
    const float cutoffCoefTarget =
        1.0f - std::exp(-6.2831853f * cutoffHz / float(sampleRate_));

    const float earlyTarget = p.earlyLevel;
    const float tailTarget = p.tailLevel;
    const float masterTarget = dbToGain(p.masterGainDb);
    const float airTarget = std::clamp(p.breath, 0.0f, 1.0f);
    // The air/HF shelf is a tone control and stays; only the breath-NOISE
    // bed is a modeled imperfection, so only it is scaled by `clean`.
    const float noiseTarget = airTarget * cleanScale(p);

    // Room parameter updates only when changed (cheap checks, RT-safe).
    const float predelayMs = 10.0f + p.spaceSize * 18.0f;
    if (p.spaceSize != lastSize_ || p.spaceDecay != lastDecay_ ||
        p.spaceDamping != lastDamp_) {
        lastSize_ = p.spaceSize; lastDecay_ = p.spaceDecay; lastDamp_ = p.spaceDamping;
        cathedral_.setParams(p.spaceSize, p.spaceDecay, p.spaceDamping, 0.3f);
    }
    if (predelayMs != lastPredelay_) {
        lastPredelay_ = predelayMs;
        early_.setSpace(predelayMs, 0.35f + 0.45f * p.spaceDamping);
    }

    const float smFast = smoothCoef(sampleRate_, 12.0f);
    const float smSlow = smoothCoef(sampleRate_, 40.0f);

    // Ensemble breathing: a very slow, gentle collective level wave.
    const float clean = cleanScale(p);
    const float breatheDepth = 0.012f * p.ensemble * clean;
    const float breatheInc = float(2.0 * 3.14159265 * 0.09 / sampleRate_);

    // Breath/air: shelf split at ~4 kHz; breath-noise bed follows the choir's
    // own envelope so silence stays silent.
    const float airSplitCoef = 1.0f - std::exp(-6.2831853f * 4000.0f / float(sampleRate_));
    const float envAttack = smoothCoef(sampleRate_, 15.0f);
    const float envRelease = smoothCoef(sampleRate_, 220.0f);
    const float noiseShapeCoef = 1.0f - std::exp(-6.2831853f * 2600.0f / float(sampleRate_));

    const float widthAmt = std::clamp(p.width, 0.0f, 2.0f);

    // --- per-sample dry chain ----------------------------------------------
    for (int f = 0; f < n; ++f) {
        smDynGain_ += smFast * (dynGainTarget - smDynGain_);
        smExprGain_ += smFast * (exprGainTarget - smExprGain_);
        smCutoffCoef_ += smSlow * (cutoffCoefTarget - smCutoffCoef_);
        smEarly_ += smSlow * (earlyTarget - smEarly_);
        smTail_ += smSlow * (tailTarget - smTail_);
        smMaster_ += smSlow * (masterTarget - smMaster_);
        smAir_ += smSlow * (airTarget - smAir_);
        smNoise_ += smSlow * (noiseTarget - smNoise_);

        float l = dryL_[size_t(f)];
        float r = dryR_[size_t(f)];

        // Width (mid/side).
        const float mid = 0.5f * (l + r);
        const float side = 0.5f * (l - r) * widthAmt;
        l = mid + side;
        r = mid - side;

        // Dynamics timbre filter.
        lpL_ += smCutoffCoef_ * (l - lpL_);
        lpR_ += smCutoffCoef_ * (r - lpR_);
        l = lpL_;
        r = lpR_;

        // Air: lift what the timbre filter keeps above the shelf split.
        airLpL_ += airSplitCoef * (l - airLpL_);
        airLpR_ += airSplitCoef * (r - airLpR_);
        l += smAir_ * 1.3f * (l - airLpL_);
        r += smAir_ * 1.3f * (r - airLpR_);

        // Breath-noise bed, gated by the choir's own level.
        const float level = 0.5f * (std::abs(l) + std::abs(r));
        envFollow_ += (level > envFollow_ ? envAttack : envRelease) * (level - envFollow_);
        if (smNoise_ > 0.0f) {
            noiseState_ = noiseState_ * 1664525u + 1013904223u;
            const float white = float(noiseState_ >> 9) * (1.0f / 8388608.0f) - 1.0f;
            noiseLp_ += noiseShapeCoef * (white - noiseLp_);
            const float breathNoise = (white - noiseLp_) * envFollow_ * smNoise_ * 0.35f;
            l += breathNoise;
            r -= breathNoise * 0.8f;
        }

        float gain = smDynGain_ * smExprGain_;
        if (breatheDepth > 0.0f) {
            breathePhase_ += breatheInc;
            if (breathePhase_ > 6.2831853f) breathePhase_ -= 6.2831853f;
            gain *= 1.0f + breatheDepth * std::sin(breathePhase_);
        }
        l *= gain;
        r *= gain;

        dryL_[size_t(f)] = l;
        dryR_[size_t(f)] = r;
        sendL_[size_t(f)] = l;
        sendR_[size_t(f)] = r;
    }

    // --- cathedral ----------------------------------------------------------
    early_.process(sendL_.data(), sendR_.data(), earlyL_.data(), earlyR_.data(), n);
    // The tail is fed by direct sound + early reflections (coherent space).
    for (int f = 0; f < n; ++f) {
        sendL_[size_t(f)] = sendL_[size_t(f)] * 0.8f + earlyL_[size_t(f)] * 0.6f;
        sendR_[size_t(f)] = sendR_[size_t(f)] * 0.8f + earlyR_[size_t(f)] * 0.6f;
    }
    cathedral_.process(sendL_.data(), sendR_.data(), tailL_.data(), tailR_.data(), n);

    for (int f = 0; f < n; ++f) {
        float l = (dryL_[size_t(f)] + earlyL_[size_t(f)] * smEarly_ +
                   tailL_[size_t(f)] * smTail_) * smMaster_;
        float r = (dryR_[size_t(f)] + earlyR_[size_t(f)] * smEarly_ +
                   tailR_[size_t(f)] * smTail_) * smMaster_;
        if (p.limiter) {
            // Continuous soft saturation: ~transparent at low level, caps at ±1.
            l = std::tanh(l);
            r = std::tanh(r);
        }
        outL[f] = l;
        outR[f] = r;
    }
    for (int f = n; f < frames; ++f) { outL[f] = 0.0f; outR[f] = 0.0f; }
}

} // namespace sapp::choir
