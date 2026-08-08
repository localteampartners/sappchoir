# ARCHITECTURE — sappchoir

<!-- UPDATE WHEN: tech stack changes, a component is added/removed, data flow changes, or a major directory is renamed -->

## Tech stack

- **Language / runtime:** C++20, CMake ≥ 3.24
- **Framework:** JUCE 8.0.15 (plugin/UI only; core is framework-free)
- **Engine:** SappSounds (`Sapp::Sounds`) via `add_subdirectory(../sappsounds)`
  (sibling checkout; FetchContent fallback from GitHub)
- **Tests:** Catch2 v3.7.1
- **Database:** none

## Components

- `src/core/` (`sappchoir_core`, no JUCE):
  - `VowelLayers` — vowel policy. `makeVowelInstrument()` quadruples attack
    regions, formant-filters each sample (3 parallel RBJ bandpass + dry
    blend, RMS-matched) into oo/oh/ah/eh, and registers live CC 20
    crossfades (`xfin/xfout` windows 0-42-85-127). Skips instruments that
    already ship CC 20 crossfades. Offline, deterministic.
  - `ChoirEngine` — product policy over `sapp::sounds::PlaybackEngine`:
    Vowel CC injection (parameter → CC 20, incoming CC authoritative), CC1
    dynamics (−18 dB + LP timbre), CC11 expression, breath (HF shelf +
    envelope-gated noise bed), ensemble (randomTuneCents 0–14¢ + slow level
    breathing), width (M/S), legato policy, cathedral sends, tanh limiter.
  - `CathedralReverb.h` — `EarlyReflections` (late dark taps, predelay from
    size) + `CathedralReverb` (8-line Householder FDN, long base delays,
    T60 0.5–20 s, heavy damping range, slow modulation).
  - `ChoirRender` — deterministic offline render; applies SappLink CCs to
    params mid-render exactly like the plugin path.
  - `SappLinkCCMap` — the one CC table both paths share.
- `src/plugin/` — `SappChoirProcessor` (APVTS, knob→CC bridges, SappLink CC
  slews, async SFZ load + vowel generation off the audio thread) +
  `SappChoirEditor` (candlelit cathedral, `VowelWheel`).
- `src/cli/` — `sappchoir` binary; JSON contracts in docs/agent_api.md.
- `tools/uishot/` — offscreen editor PNG + `--cctest` plugin-path proof.
- `tests/` — 28 unit tests incl. SappLink manifest drift guard.
- `src/plugin/UpdateManager.h` — in-plugin updater (background
  thread): GitHub latest-release check vs JucePlugin_VersionString,
  platform-asset download, install (SappChoir.vst3/.component on
  macOS + xattr -rc; Windows rename-trick swap), standalone
  self-relaunch on macOS. `src/core/VersionCompare.h` does the
  semver-ish tag comparison.

## Data flow

MIDI → ChoirEngine: CC1/CC11 live-override params; CC20 forwards to the
sampler where per-voice vowel crossfades morph; param `vowel` changes are
re-injected as CC20 (once, quantized). Sampler renders dry → width →
dynamics timbre LP → air shelf + breath noise → ensemble breathing → early
reflections → FDN tail → master/limiter.

Layout: 940×620 plugin editor; core has no UI deps. Ownership boundary:
SappSounds owns SFZ/voices/crossfades; sappchoir owns choir policy.
