# sappchoir

<!-- UPDATE WHEN: the one-line description changes, or the repo's top-level layout changes -->

Ethereal choir & vocal-pad instrument (JUCE Standalone/VST3/AU) built on the
SappSounds engine. The signature: **live vowel morphing** — every loaded SFZ
choir grows four formant vowel layers (oo/oh/ah/eh) that crossfade on CC 20
*while notes sound*. Plus breath/air, ensemble size, a long cathedral room,
an agent-facing CLI/JSON API, and a SappLink manifest for sapptune.

## Quickstart

```bash
# Core + CLI + tests (fast loop)
./verify.sh

# Plugin (Standalone / VST3 / AU) — reuse a local JUCE checkout if you have one
cmake -S . -B build-plugin -DCMAKE_BUILD_TYPE=Release -DSAPPCHOIR_BUILD_TESTS=OFF \
      -DFETCHCONTENT_SOURCE_DIR_JUCE=$HOME/apps/sappsynth/build/_deps/juce-src
cmake --build build-plugin -j8

# Choir samples (Sonatina Chorus + CC0 voice sets)
~/apps/sappsounds/scripts/fetch-library.sh get sonatina
~/apps/sappsounds/scripts/fetch-library.sh get freepats-synth-choir
~/apps/sappsounds/scripts/fetch-library.sh get legato-vocal

# A rendered demo (SATB progression, CC1 swells, oo→ah→oo vowel journey)
python3 scripts/make_choir_demo.py
```

Requires the sibling engine checkout at `../sappsounds` (or it is fetched
from GitHub).

## Layout

- `src/core/` — framework-free product core: `ChoirEngine`, `VowelLayers`
  (formant-layer generation), `CathedralReverb`, `ChoirRender`, SappLink map
- `src/plugin/` — JUCE processor + candlelit-cathedral editor (vowel wheel)
- `src/cli/` — `sappchoir` agent CLI ([docs/agent_api.md](docs/agent_api.md))
- `tools/uishot/` — offscreen editor screenshot + `--cctest` SappLink proof
- `tests/` — Catch2 unit tests + vendored SappLink manifest
- `docs/` — [agent_api.md](docs/agent_api.md), [sapplink.md](docs/sapplink.md)

## Project documentation

All orientation docs live in [`_project/`](_project/). Start with
[_project/README.md](_project/README.md) — it's a 1-page index into everything
else (spec, architecture, current state, runbook, decisions, etc.).

If you're an agent opening this repo, read [CLAUDE.md](CLAUDE.md) first.
