#pragma once
// SappLink v1 CC-in mapping for SappChoir (framework-free).
//
// Source of truth: ~/apps/sapptune/sapplink/manifests/sappchoir.json —
// the unit test asserts this table matches the vendored copy in tests/data/.
// Parameter identity = the plugin's stable APVTS parameter IDs.
//
// Deliberately ABSENT from this table (engine-native, existing behavior):
//   CC 1  → dynamics    (choir swell, handled by ChoirEngine)
//   CC 11 → expression  (handled by ChoirEngine)
//   CC 64 → sustain     (real pedal semantics in SappSounds)
//   pitch bend, keyswitch notes (articulations are switched by notes)
//
// CC 20 (vowel) IS in the table: the parameter follows the CC, and the raw
// CC also reaches the sampler where the SFZ crossfade layers do the morph.

#include <array>

#include "ChoirEngine.h"

namespace sapp::choir::sapplink {

enum class Curve { Linear, Log };

struct CCMapping {
    int cc;
    const char* paramId;           // stable APVTS parameter ID, verbatim
    float ChoirParams::* field;    // same parameter in the core struct
    float lo, hi;                  // engineering units at CC 0 and CC 127
    Curve curve;
};

inline constexpr int kNumMappings = 11;
const std::array<CCMapping, kNumMappings>& mappings();

// nullptr if this CC is not part of the SappLink contract.
const CCMapping* findMapping(int cc);

// CC value 0..127 → engineering units through the mapping's curve.
float ccToEngineering(const CCMapping& mapping, int ccValue);

// Offline/CLI path: apply a mapped CC to the params struct.
// Returns true if the CC was part of the mapping.
bool applyCcToParams(ChoirParams& params, int cc, int ccValue);

} // namespace sapp::choir::sapplink
