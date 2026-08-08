# CHANGELOG — sappchoir

<!-- UPDATE WHEN: you ship a meaningful change — feature, fix, migration, dependency bump that users/operators would care about. Trivial refactors don't belong here. -->

Newest first. Format: `## YYYY-MM-DD — short title`, then bullets.

---

## 2026-08-07 — v0.3.0
- In-plugin UPDATE button: daily GitHub release check (click the version
  number to check on demand); one click downloads and installs the new
  build (macOS: plug-in folders + quarantine cleared; Windows: loaded
  .vst3 swapped via rename), standalone relaunches itself on macOS.
- Plugin version now tracks release tags (0.3.0).

## 2026-08-06 — GET SOUNDS: in-plugin library downloads

- SoundsPanel ported from sapporchestra: GET SOUNDS header button opens a
  cathedral-styled overlay with one-click download → extract (juce::URL +
  juce::ZipFile; tar.gz via system tar) → progress → rescan.
- Registry: FreePats Synth Pad Choir (7 MB, CC0), Legato Vocal Set
  (160 MB, CC0), Sonatina Symphonic Orchestra chorus (2.6 GB, CC Sampling
  Plus) — all zip, via codeload.github.com.
- Installed-voices browser: recursive .sfz scan of the shared Sapp samples
  root (skips includes/), category + text filter, double-click to load;
  loads go through SappChoirProcessor::loadSfzInstrument, so downloaded
  choirs get vowel-morph layers automatically.
- UiShot: new `--sounds` flag snapshots the overlay.

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
