# CHANGELOG — sappchoir

<!-- UPDATE WHEN: you ship a meaningful change — feature, fix, migration, dependency bump that users/operators would care about. Trivial refactors don't belong here. -->

Newest first. Format: `## YYYY-MM-DD — short title`, then bullets.

---

## 2026-08-06 — v0.1.0: initial release

- Core: ChoirEngine (CC1 dynamics, CC11 expression, breath, ensemble,
  width, legato policy), CathedralReverb (early + 8-line FDN, T60 to 20 s).
- **Vowel morphing:** generated formant layers (oo/oh/ah/eh) with live
  CC 20 crossfades via SappSounds xfin/xfout; native-layer passthrough.
- Plugin: Standalone/VST3/AU (JUCE 8.0.15), candlelit cathedral editor,
  vowel morph wheel, keyswitch keyboard, UiShot (+ --cctest).
- Agent CLI: inspect/validate/params/vowels/scan/render (JSON, seeded).
- SappLink v1: manifest (sapptune) + vendored drift-guarded copy.
- 28 Catch2 tests; verify.sh; Sonatina demo script.
- sappsounds fetch-library.sh: registered freepats-synth-choir +
  legato-vocal (CC0 voice sets).
