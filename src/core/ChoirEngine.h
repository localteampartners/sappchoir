#pragma once
// ChoirEngine — SappChoir's product policy wrapped around the generic
// sapp::sounds::PlaybackEngine.
//
// SappSounds owns: SFZ, samples, voices, mapping, live CC crossfades (the
// vowel morph itself), keyswitches, legato.
// SappChoir owns (here): the Vowel CC injection policy, CC1 dynamics
// (level + timbre), CC11 expression, breath/air, ensemble size (random
// tune + width + slow breathing), the cathedral room, master output policy.
//
// Framework-independent: no JUCE. The JUCE plugin and the CLI both drive this.

#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

#include <sapp/sounds/InstrumentDefinition.h>
#include <sapp/sounds/PlaybackEngine.h>

#include "CathedralReverb.h"
#include "VowelLayers.h"

namespace sapp::choir {

struct ChoirParams {
    // Voice
    float vowel = 0.35f;          // 0 oo … 1 eh, follows the Vowel CC (20)
    float dynamics = 0.65f;       // 0..1, follows CC1 (level + timbre)
    float expression = 1.0f;      // 0..1, follows CC11
    float breath = 0.25f;         // air: HF lift + breath-noise bed
    // Ensemble
    float ensemble = 0.5f;        // size: detune spread + slow level breathing
    float width = 1.2f;           // 0 mono .. 2 wide
    // Space (cathedral)
    float earlyLevel = 0.28f;
    float tailLevel = 0.45f;
    float spaceSize = 1.15f;      // 0.2..1.5
    float spaceDecay = 6.5f;      // seconds, 1..20
    float spaceDamping = 0.55f;
    // Performance policy
    float legato = 1.0f;          // >= 0.5 = legato level 2 on (CC 68)
    // Cleanliness — the suite-wide SappLink `clean` control (CC 3).
    // 0 = every modeled imperfection as designed (the historical behavior);
    // 1 = none. See cleanScale() for exactly what it scales.
    float clean = 0.0f;
    // Output
    float masterGainDb = 0.0f;
    bool limiter = true;
    int quality = 1;              // 0 draft, 1 normal/high
};

/// THE `clean` contract for this engine. SappChoir's modeled-imperfection
/// sources are the breath-noise bed (`breath`) and the ensemble
/// humanization that `ensemble` drives — per-note random detune and the slow
/// collective level wave. All of them are multiplied by this scale.
///
/// Audited and deliberately NOT scaled:
///   * the breath HF/air shelf lift — a tone control, not wear;
///   * ensemble *width* and voice count — musical size, not imperfection;
///   * the cathedral room — architecture, not wear;
///   * round-robin / velocity variation — that comes from the sample library.
///
/// `clean` never scales the musical signal, so clean=1 must still sound.
inline float cleanScale(const ChoirParams& p) noexcept
{
    const float clean = p.clean < 0.0f ? 0.0f : (p.clean > 1.0f ? 1.0f : p.clean);
    return 1.0f - clean;
}

class ChoirEngine {
public:
    ChoirEngine();

    // --- control thread -----------------------------------------------------
    void prepare(double sampleRate, int maxBlockFrames);
    void setInstrument(sapp::sounds::InstrumentPtr instrument);
    void collectRetired();
    sapp::sounds::InstrumentPtr currentInstrument() const;

    // Articulation policy: press the articulation's keyswitch on the next block.
    void selectArticulation(int index);

    void setParams(const ChoirParams& params);   // copied atomically
    ChoirParams params() const;

    void resetSequences();
    void reseed(uint32_t seed);

    const sapp::sounds::PlaybackEngine& sampler() const { return sampler_; }
    sapp::sounds::PlaybackEngine& sampler() { return sampler_; }

    // --- audio thread -------------------------------------------------------
    // Replaces buffer contents (not additive). Events sorted by frame.
    void process(const sapp::sounds::MidiEvent* events, int eventCount,
                 float* outL, float* outR, int frames) noexcept;

private:
    void applyQuality(const ChoirParams& p) noexcept;

    sapp::sounds::PlaybackEngine sampler_;
    EarlyReflections early_;
    CathedralReverb cathedral_;

    // Double-buffered params: control writes inactive slot then flips.
    ChoirParams paramSlots_[2];
    std::atomic<int> paramIndex_{0};

    std::atomic<int> pendingArticulationKeyswitch_{-1};

    // Live controller state (audio thread). CC1/CC11 persistently override
    // the parameter values once received. The Vowel CC drives the sampler's
    // crossfades directly; lastSentVowelCc_ tracks the sampler-visible value
    // so a moved vowel parameter is re-injected as a CC exactly once.
    float liveDynamics_ = -1.0f, liveExpression_ = -1.0f;
    int lastSentVowelCc_ = -1;

    // Smoothed audio-thread state.
    float smDynGain_ = 0.5f, smExprGain_ = 1.0f, smCutoffCoef_ = 1.0f;
    float smEarly_ = 0.3f, smTail_ = 0.4f, smMaster_ = 1.0f, smAir_ = 0.0f;
    float smNoise_ = 0.0f;                // breath-noise bed level (air × clean)
    float lpL_ = 0.0f, lpR_ = 0.0f;       // dynamics timbre filter
    float airLpL_ = 0.0f, airLpR_ = 0.0f; // air shelf split points
    float noiseLp_ = 0.0f;                // breath-noise shaping
    float envFollow_ = 0.0f;              // breath-noise gate
    float breathePhase_ = 0.0f;           // ensemble slow breathing
    uint32_t noiseState_ = 0x9E3779B9u;

    // Scratch buffers (allocated in prepare).
    std::vector<float> dryL_, dryR_, sendL_, sendR_, earlyL_, earlyR_, tailL_, tailR_;

    double sampleRate_ = 48000.0;
    int maxBlock_ = 0;
    float lastSize_ = -1.0f, lastDecay_ = -1.0f, lastDamp_ = -1.0f, lastPredelay_ = -1.0f;
    int lastQuality_ = -1;
    float lastEnsembleCents_ = -1.0f;
    int lastLegato_ = -1;
};

} // namespace sapp::choir
