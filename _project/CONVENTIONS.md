# CONVENTIONS — sappchoir

<!-- UPDATE WHEN: you learn a non-obvious workflow fact (deploy quirk, version pin, "never do X") -->

- **JUCE pinned at 8.0.15** — same tag across sappsynth/sapporchestra/
  sappchoir; reuse the checkout at `~/apps/sappsynth/build/_deps/juce-src`
  via `FETCHCONTENT_SOURCE_DIR_JUCE` to avoid a 300 MB clone.
- Two build trees: `build/` (core+CLI+tests, plugin OFF — the verify loop),
  `build-plugin/` (plugin+UiShot, tests OFF). Keep it that way; mixing makes
  verify slow.
- Parameter IDs are compatibility contracts (APVTS + SappLink + CLI docs) —
  never rename/renumber.
- If the SappLink manifest changes, update sapptune's copy, the vendored
  `tests/data/sapplink-manifest.json`, and `src/core/SappLinkCCMap.cpp`
  together — the drift test fails otherwise, by design.
- The core (`src/core`, `src/cli`, tests) must never include JUCE.
- Warnings are errors-in-spirit: sappchoir targets build clean under
  `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion`.
- No AI co-author trailers in commits.
