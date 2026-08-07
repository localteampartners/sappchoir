# CURRENT STATE — sappchoir

<!-- UPDATE WHEN: a feature ships, a deploy happens, something breaks, or something gets fixed. This file answers "what's the project like *right now*?" -->

**Last verified:** 2026-08-06

---

## What's built and working

- v0.1.0 complete: core engine, vowel morphing (generated formant layers on
  CC 20, live crossfades), cathedral room, breath/ensemble/width controls.
- Plugin builds Standalone/VST3/AU (JUCE 8.0.15); candlelit cathedral UI
  with vowel morph wheel; UiShot screenshot + `--cctest` pass.
- Agent CLI: inspect/validate/params/vowels/scan/render — JSON, seeded,
  deterministic (same seed ⇒ bit-identical WAV).
- SappLink v1: manifest in sapptune, vendored copy + drift-guard test; CC
  20 vowel / 21 breath / 22 ensemble / space CCs; CC1/11/64 engine-native.
- 28 Catch2 tests green; `./verify.sh` passes (<60 s warm).
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
