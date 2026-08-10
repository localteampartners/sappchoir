# CONVENTIONS — sappchoir

<!-- UPDATE WHEN: you learn a non-obvious workflow fact (deploy quirk, version pin, "never do X") -->

- **JUCE pinned at 8.0.15** — same tag across sappsynth/sapporchestra/
  sappchoir; reuse the checkout at `~/apps/sappsynth/build/_deps/juce-src`
  via `FETCHCONTENT_SOURCE_DIR_JUCE` to avoid a 300 MB clone.
- Two build trees: `build/` (core+CLI+tests, plugin OFF — the fast inner
  loop), `build-plugin/` (plugin + UiShot + the headless harness). `verify.sh`
  builds BOTH: never verify with the plugin target off (sappkeys #1 —
  green tests while the installed binary stayed stale).
- **Never load an instrument from the JUCE message thread** (issue #1). A
  headless host has a MessageManager nobody pumps, so `callAsync` and
  `juce::Timer` silently never fire. Everything that installs an instrument
  goes through the processor's loader thread.
- Parameter IDs are compatibility contracts (APVTS + SappLink + CLI docs) —
  never rename/renumber.
- If the SappLink manifest changes, update sapptune's copy, the vendored
  `tests/data/sapplink-manifest.json`, and `src/core/SappLinkCCMap.cpp`
  together — the drift test fails otherwise, by design.
- The core (`src/core`, `src/cli`, tests) must never include JUCE.
- Warnings are errors-in-spirit: sappchoir targets build clean under
  `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion`.
- No AI co-author trailers in commits.
