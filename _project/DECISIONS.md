# DECISIONS — sappchoir

<!-- UPDATE WHEN: you make a non-obvious choice (library pick, architectural pattern, tradeoff). One entry per decision, newest at top. -->

---

## 2026-08-06 — Vowel CC is BOTH SappLink-mapped and sampler-native

CC 20 slews the `vowel` APVTS parameter (SappLink path) *and* the raw CC
reaches the sampler where SFZ crossfades morph per-voice. To avoid the two
fighting, ChoirEngine re-injects CC 20 only when the quantized *parameter*
value changes, and records incoming CC 20 as already-sent. One controller,
both consumers, no feedback loop. Alternative (engine-native only, like
CC1) would have hidden `vowel` from the manifest.

## 2026-08-06 — Generate vowel layers at load, not at build/install

`makeVowelInstrument()` runs in the async load thread: 3 parallel RBJ
bandpass biquads + 22% dry blend, RMS-matched, per sample per vowel.
Deterministic, no cache files on disk, works for any SFZ the user loads.
Cost: ~4× sample RAM + a moment at load. Libraries already shipping CC 20
crossfades pass through untouched (`hasVowelLayers` guard, idempotent).

## 2026-08-06 — Crossfade windows 0/42/85/127, adjacent xfin==xfout

Each vowel's fade-in range equals the previous vowel's fade-out range, so
every CC value sums to constant power (SappSounds' default equal-power
curve). Verified by tiling assertions in test_vowel.cpp.

## 2026-08-06 — Cathedral = retuned copy of sapporchestra's Reverb.h, not a shared lib

Same FDN topology (8 lines, Householder), but base delays ~1.35×, decay
clamp 0.5–20 s, damping range to 0.92, slower LFOs. Sharing a DSP lib with
sapporchestra would couple release cycles for ~150 lines of header; the
suite treats product DSP as product-owned (same reason SappSounds has no
SappLink code).

## 2026-08-06 — Breath is engine DSP, not sample layers

Air = HF shelf on the post-timbre signal + envelope-follower-gated filtered
noise. No breath samples exist in the free libraries; synthesizing keeps it
library-independent and silence-safe (no notes → no hiss).

## 2026-08-06 — kept sapporchestra's stage/pan out

A choir sits where it sits; ensemble/width/space cover the imaging. Fewer
params keeps the vowel wheel the undisputed hero control.

## 2026-08-07 — Self-update via GitHub releases, versioned by the CMake project

The plugin updates itself from the repo's *latest GitHub release* rather
than a separate update feed: CI already attaches Windows-x64 and
macOS-universal zips to every tag, so the release IS the feed. The
installed version is `JucePlugin_VersionString`, which JUCE derives from
`project(SappChoir VERSION ...)` — therefore the CMake version MUST be
bumped with every release tag (RUNBOOK rule) or the updater goes blind.
Daily check throttled through the shared Sapp settings file (one file for
the whole product family, per-product key `lastUpdateCheck-sappchoir`).
Windows can't overwrite a loaded DLL but can rename it: old .vst3 is
parked as `.old-<tag>` and the new one copied in, with rollback on failure.
