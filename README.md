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
- `tools/headless/` — `sappchoir-headless`: the station host (no editor, no
  JUCE dispatch loop); `selftest` is the issue #1 regression
- `tests/` — Catch2 unit tests + vendored SappLink manifest
- `docs/` — [agent_api.md](docs/agent_api.md), [sapplink.md](docs/sapplink.md)

## Project documentation

All orientation docs live in [`_project/`](_project/). Start with
[_project/README.md](_project/README.md) — it's a 1-page index into everything
else (spec, architecture, current state, runbook, decisions, etc.).

If you're an agent opening this repo, read [CLAUDE.md](CLAUDE.md) first.

## Where releases are built

Tags are built by a **self-hosted GitHub Actions runner on the Windows
machine** (`desktop-14886fp`), not by GitHub's hosted runners — hosted minutes
are billed and the account is currently blocked. Windows jobs read:

```yaml
runs-on: ${{ vars.WINDOWS_RUNNER || 'windows-latest' }}
```

so the repo variable `WINDOWS_RUNNER=self-hosted` sends builds to that
machine, and deleting the variable sends them back to GitHub. No workflow
edits either way.

**Every repo needs its own runner.** The account is a GitHub *user*, not an
organisation, and user accounts can't share runners across repos — so each
repo gets its own registration (its own folder and Windows service) on the
same machine. The prerequisites are installed once and shared: Git, CMake
3.24+, and Visual Studio 2022 Build Tools with the "Desktop development with
C++" workload.

Full setup, including the per-repo registration steps:
[sapptune/RUNNER.md](https://github.com/localteampartners/sapptune/blob/master/RUNNER.md).

**Builds are Windows-only** — macOS jobs were removed on 2026-08-08.
