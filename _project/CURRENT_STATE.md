# CURRENT STATE — sappchoir

<!-- UPDATE WHEN: a feature ships, a deploy happens, something breaks, or something gets fixed. This file answers "what's the project like *right now*?" -->

**Last verified:** 2026-08-11

---

## Shipped 2026-08-11 — v0.8.0 (`libraryReady` drops on a MIDI program change)

- Audit of sappkeys #4 against this repo. The `instrument` parameter path was
  already honest; the MIDI program-change branch of `processBlock()` was not —
  it queued the select on the audio thread and left readiness to the loader
  thread's next pass (~5 ms), so a host that sent the program change and
  polled immediately read the previous library's "ready" and could render into
  the load. Now cleared on the calling thread, right where the select is
  stored.
- 5 new headless checks (19 total) cover a mid-session swap through both the
  `instrument` parameter and MIDI program change; they fail on the previous
  build. `./verify.sh` green.
- Not tagged — the release is driven separately. The CI guard checks the tag
  against `project(... VERSION ...)` in CMakeLists.txt, now 0.8.0.

## Shipped 2026-08-10 — v0.7.0 (headless silence fixed, issue #1)

- Instrument loading no longer touches the JUCE message thread. The
  processor owns a **loader thread**; parameter selections, program
  changes, state restores and the construction diagnostic are queued
  `LoadJob`s installed there. Previously `MessageManager::callAsync` +
  a `juce::Timer` owned the whole path, so in a headless host (which has a
  MessageManager nobody pumps) NOTHING loaded — not even the built-in
  choir — and SappChoir rendered digital silence. That is the sappradio
  station's -61.2 dBFS report; the residue it measured was the downstream
  chain, not this plugin.
- Read-only `libraryReady` host parameter: poll it instead of a blind
  settle window. Diagnostic log lines `SappChoir-build:`,
  `SappChoir-instrument:` (incl. `MISSING`), `SappChoir-audio-source:` —
  host logger, stderr on Windows, and `$SAPP_CHOIR_LOG`.
- Suite-wide `clean` parameter (id `clean`, CC 3, default 0) scales the
  breath-noise bed and the ensemble humanization.
- `tools/headless/` — the `sappchoir-headless` station harness (no editor,
  no dispatch loop). Its `selftest` (14 checks) is the regression, run by
  CTest and by `./verify.sh`, which now builds the plugin target too.

## Shipped 2026-08-07 — v0.3.0 (in-plugin updater)

- Footer version button checks GitHub daily (or on click); UPDATE
  button downloads + installs the newest release (macOS: plug-in
  folders + quarantine clear; Windows: rename-trick swap of the
  loaded .vst3). Throttle key `lastUpdateCheck-sappchoir` in the shared
  Sapp settings file.
- v0.3.0 GitHub release carries CI-built Windows-x64 and
  macOS-universal zips (SappChoir VST3/AU/Standalone). Same code path
  end-to-end verified in sappkeys 2026-08-07.
- CMake `project()` VERSION must be bumped with every release tag
  (the updater compares JucePlugin_VersionString to the tag).
- Build dirs (`build/`, `build-plugin/`) no longer tracked in git.

## What's built and working

- v0.1.0 complete: core engine, vowel morphing (generated formant layers on
  CC 20, live crossfades), cathedral room, breath/ensemble/width controls.
- Plugin builds Standalone/VST3/AU (JUCE 8.0.15); candlelit cathedral UI
  with vowel morph wheel; UiShot screenshot + `--cctest` pass.
- Agent CLI: inspect/validate/params/vowels/scan/render — JSON, seeded,
  deterministic (same seed ⇒ bit-identical WAV).
- SappLink v1: manifest in sapptune, vendored copy + drift-guard test; CC
  3 clean / 20 vowel / 21 breath / 22 ensemble / space CCs; CC1/11/64
  engine-native.
- Host-automatable SFZ selection (sapptune #20): `instrument` choice param
  (appended last, automation indices hold) enumerates the library via
  `<samplesRoot>/.sapp-sfz-index.json` (ordering contract in
  src/core/SfzLibrary, case-insensitive by label); bank-select + program
  change loads by entry index; state stays path-based; CLI `sfz-index`
  prints name→choice→normalized; rescans take effect next instantiation.
- 40 Catch2 tests green plus the 14-check headless selftest;
  `./verify.sh` passes (builds core+CLI+tests AND the plugin+harness).
- Demo: `scripts/make_choir_demo.py` renders an 8-bar SATB progression with
  Sonatina Mixed Chorus + oo→ah→oo vowel journey.
- Sample libraries: Sonatina Chorus (local), freepats-synth-choir +
  legato-vocal registered in sappsounds fetch-library.sh (CC0, fetched).
- GET SOUNDS panel (ported from sapporchestra): in-plugin one-click
  download → extract → rescan of the three vocal libraries, plus an
  installed-SFZ browser (~/Samples, shared Sapp samples root, filter +
  double-click to load — loads route through vowel-layer generation).
  UiShot `--sounds` renders the overlay for UI verification.

## Known issues

- Generated vowel layers 4× RAM per instrument (originals kept for release
  triggers). Fine for choir-sized SFZs; large keyswitch libraries get big.
- Formant filtering assumes a sung/voiced source; percussive content will
  sound resonant (use `--raw`).
- VSCO 2 CE has no vocal content (verified) — not used by sappchoir.

## Deploy state

Local builds only; repo at github.com/localteampartners/sappchoir. No VPS,
no monitor, no secrets.
