# TODO — sappchoir

<!-- UPDATE WHEN: a task is added, completed, or re-prioritized -->

Short running task list. For "what exists *right now*," see [CURRENT_STATE.md](CURRENT_STATE.md).

---

## Next up (doing soon, in order)

1. Play-test the vowel morph with Sonatina Mixed/Large Chorus in the DAW;
   tune formant gains/bandwidths per library if needed.
2. Presets (vowel journeys + space settings) exposed via program change in
   the SappLink manifest.
3. Per-vowel `sw_label`-style naming when a library ships native vowel
   layers on other CCs (remap → CC 20).

## Later / maybe

- Shimmer: subtle octave-up send into the cathedral (see ROADMAP).
- Solo-voice mode using legato-vocal library (vowel transitions + syllables).
- Sample streaming for very large choir libraries.

## Done (recent)

- 2026-08-10 — v0.7.0, issue #1: instrument loading moved off the JUCE
  message thread onto a loader thread (headless renders were silent);
  `libraryReady` parameter, `SappChoir-audio-source:` logging, suite-wide
  `clean` (CC 3), `sappchoir-headless` harness + regression.
- 2026-08-06 — GET SOUNDS panel: in-plugin vocal-library downloads +
  installed-SFZ browser (ported from sapporchestra).
- 2026-08-06 — v0.1.0: engine, vowel morph, cathedral, plugin UI, CLI,
  SappLink, tests, demo, repo published.

- [ ] One manual UPDATE-button click on a Windows machine (rename-trick
      .vst3 swap is untested on real Windows; macOS path verified).
