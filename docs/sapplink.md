# SappLink CC-in (sappchoir)

SappChoir implements SappLink v1 MIDI CC-in so sapptune-generated clips can
drive its parameters. Protocol: `~/apps/sapptune/sapplink/PROTOCOL.md`.
**Source-of-truth manifest:** `~/apps/sapptune/sapplink/manifests/sappchoir.json`
(authored 2026-08-06 directly from the plugin's real APVTS ranges). A
vendored copy lives at
[tests/data/sapplink-manifest.json](../tests/data/sapplink-manifest.json);
`tests/unit/test_sapplink.cpp` fails if the in-code table and the vendored
manifest ever drift. If sapptune's manifest changes, update the vendored copy
and `src/core/SappLinkCCMap.cpp` together.

## The mapping

| CC | Parameter ID | Range (engineering) | Curve |
|---|---|---|---|
| 7 | `masterGain` | −24 … 12 dB | linear |
| 14 | `earlyLevel` | 0 … 1 | linear |
| 15 | `spaceDecay` | 1 … 20 s | log |
| 18 | `width` | 0 … 2 | linear |
| 19 | `spaceDamping` | 0 … 1 | linear |
| 20 | `vowel` | 0 … 1 (oo → eh) | linear |
| 21 | `breath` | 0 … 1 | linear |
| 22 | `ensemble` | 0 … 1 | linear |
| 68 | `legato` | 0 … 1 (≥0.5 on) | linear |
| 91 | `tailLevel` | 0 … 1 | linear |
| 92 | `spaceSize` | 0.2 … 1.5 | linear |

CC 0→127 maps onto the range through the curve. CCs are accepted on any
MIDI channel.

**CC 20 (vowel) is special:** besides slewing the `vowel` parameter, the raw
CC event reaches the sampler, where the vowel layers' live SFZ crossfades
(`xfin_locc20`/`xfout_hicc20`) do the actual morph per-voice. One controller,
one meaning, two consumers — parameter state and crossfade engine stay in
sync by construction (the engine re-injects the Vowel CC only when the
*parameter* moves, and treats an incoming CC as authoritative).

## Deliberately NOT in the mapping (existing behavior preserved)

- **CC 1 → dynamics** and **CC 11 → expression** — engine-native performance
  controls (ChoirEngine live-follows them).
- **CC 64** — real sustain-pedal semantics in SappSounds.
- **Pitch bend** — voice pitch, per `midi.pitchBendRangeSemitones`.
- **Keyswitch notes** — articulation switching stays note-based.
- Discrete/config parameters (`quality`, `limiter`, `articulation`) are
  host-automation/CLI-only.

## How it's routed

- **Plugin path** (`src/plugin/PluginProcessor.cpp`): mapped CCs become slew
  targets; each block the APVTS parameter moves ~15 ms toward the target via
  `setValueNotifyingHost` — the same normalized path host automation uses —
  so 7-bit steps don't zipper. The CC event is still forwarded to the engine
  (vowel crossfades, SFZ `locc/hicc` conditions keep working).
- **CLI / offline path** (`src/core/ChoirRender.cpp`): the same table updates
  `ChoirParams` during `renderChoir`, so a sapptune clip renders identically
  through `sappchoir render`.
- One table drives both: `src/core/SappLinkCCMap.{h,cpp}` (framework-free).

## Verification

- `SappChoirTests "[sapplink]"` — table↔manifest drift guard, curve
  endpoints/monotonicity, reserved CCs, offline proofs (CC 20 morphs the
  spectrum; CC 7 scales output level).
- `SappChoirUiShot --cctest` — plugin-path proof through `processBlock`:
  prints `PASS` when CC 20 brightens the rendered spectrum (oo → eh).
