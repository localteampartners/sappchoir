# CHANGELOG — sappchoir

<!-- UPDATE WHEN: you ship a meaningful change — feature, fix, migration, dependency bump that users/operators would care about. Trivial refactors don't belong here. -->

Newest first. Format: `## YYYY-MM-DD — short title`, then bullets.

---

## 2026-08-10 — v0.7.0: headless silence fixed (issue #1)
- **Root cause:** every instrument install ran on the JUCE **message
  thread** (`MessageManager::callAsync` for the load completion, a
  `juce::Timer` for applying the `instrument` choice). A plugin embedded in
  a headless host has a MessageManager that nothing pumps, so *nothing*
  loaded — not even the built-in choir. The sampler held a null instrument
  for the whole render and SappChoir emitted **digital silence**; the
  -61.2 dBFS the station measured was the downstream chain's own floor.
- **Fix:** the processor owns a **loader thread**. Parameter selections,
  program changes, state restores and the construction diagnostic are
  queued `LoadJob`s installed there, with no message loop anywhere. The
  30 Hz timer survives only to fire the editor's `onInstrumentChanged`
  hook. The loader thread is joined in the destructor (the old detached
  thread + `callAsync` closures capturing `this` were a latent crash).
- The construction diagnostic no longer writes the `instrument` parameter,
  so a selection made microseconds after instantiation cannot be reset.
- New read-only `libraryReady` host parameter (non-automatable, appended
  after every APVTS parameter, outside the APVTS so host state cannot
  restore a stale "ready"). **Poll it instead of a blind `--settle`.**
- New diagnostic log lines — `SappChoir-build:`, `SappChoir-instrument:`
  (including `MISSING` for a label that no longer resolves) and
  `SappChoir-audio-source:` when a voice batch starts from silence. They go
  to the host's JUCE logger, to stderr on Windows, and to `$SAPP_CHOIR_LOG`.
- New suite-wide **`clean`** parameter (id `clean`, CC 3, 0..1, default 0,
  appended after `instrument`): scales SappChoir's modeled-imperfection
  sources — the breath-noise bed and the ensemble humanization (per-note
  detune, slow collective level wave). Never scales the musical signal;
  audited exclusions documented at `cleanScale()` in `ChoirEngine.h`.
- New `sappchoir-headless` harness (`tools/headless/`) — the station host:
  no editor, no dispatch loop. `selftest` is the regression (14 checks, run
  by CTest and `./verify.sh`); it fails on the pre-fix code.
- `verify.sh` now builds the **plugin** as well as the tests (sappkeys #1:
  green tests, stale binary) and runs the headless selftest.
- Measured, station condition (D3–D5 chords, default parameters, no CCs,
  no dispatch loop): VPO `choir-MIXED-sustain` **-200 → -22.70 dBFS RMS**;
  Sonatina Mixed Chorus **-200 → -25.87 dBFS**; built-in default
  **-200 → -25.47 dBFS**. 40 unit tests + 14 headless checks green,
  auval PASS.

## 2026-08-09 — host-automatable `instrument` parameter (sapptune #20)
- New `instrument` AudioParameterChoice (appended LAST — all existing
  automation indices hold): enumerates every installed SFZ instrument from
  a cached index at `<samplesRoot>/.sapp-sfz-index.json`; selecting choice
  k loads library entry k-1 on the message thread (through the existing
  vowel-morph load path).
- New core module `SfzLibrary` (no JUCE, shared design with sapporchestra):
  scan / index / ordering contract — entries sorted case-insensitively by
  label. `SAPP_SFZ_ROOT` env overrides the samples root.
- MIDI bank-select (CC0/CC32) + program change selects by entry index
  (any channel; single-timbral): entry = (bank * 128) + program.
- Chosen SFZ still persists BY PATH in host state (graceful fallback when
  the file is gone); the parameter re-syncs to the loaded path.
- CLI: `sappchoir sfz-index [--root DIR] [--rescan]`; rescans take effect
  on the next plugin instantiation (choice lists are fixed live).
- Manifest: `hostParameters` + `instrumentSelect` contract in
  sapptune/sapplink/manifests/sappchoir.json (mirrored in tests/data).
- Verified: 36 unit tests green (new [sfzlib] cases incl. sort-order and
  index round-trip), headless `--sfztest` (11 checks), auval PASS,
  `--cctest` regression PASS.

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
