# DEPENDENCIES — sappchoir

<!-- UPDATE WHEN: an external service, API, or account is added/removed -->

## Code

- **SappSounds** — sibling checkout `../sappsounds` (or FetchContent from
  github.com/localteampartners/sappsounds). The sample engine.
- **JUCE 8.0.15** — FetchContent; local reuse via
  `-DFETCHCONTENT_SOURCE_DIR_JUCE=~/apps/sappsynth/build/_deps/juce-src`.
- **Catch2 v3.7.1** — FetchContent (tests only).

## Sample libraries (fetched, never committed)

- Sonatina Symphonic Orchestra — Chorus (CC Sampling Plus 1.0)
- freepats-synth-choir (CC0) · legato-vocal (CC0) — via sappsounds
  `scripts/fetch-library.sh`

## Cross-repo contracts

- SappLink manifest: `~/apps/sapptune/sapplink/manifests/sappchoir.json`
  (source of truth) ↔ `tests/data/sapplink-manifest.json` (vendored,
  drift-guarded by `SappChoirTests "[sapplink]"`).

No external services, accounts, or secrets.
