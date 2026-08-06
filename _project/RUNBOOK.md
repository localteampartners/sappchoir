# RUNBOOK — sappchoir

<!-- UPDATE WHEN: run/deploy/rollback steps change -->

## Build + test (fast loop)

```bash
./verify.sh          # configure (first run), build core+CLI+tests, run all
```

## Plugin build

```bash
cmake -S . -B build-plugin -DCMAKE_BUILD_TYPE=Release -DSAPPCHOIR_BUILD_TESTS=OFF \
      -DFETCHCONTENT_SOURCE_DIR_JUCE=$HOME/apps/sappsynth/build/_deps/juce-src
cmake --build build-plugin -j8
# artefacts: build-plugin/SappChoirPlugin_artefacts/Release/{Standalone,VST3,AU}
# AU/VST3 are auto-copied to ~/Library/Audio/Plug-Ins on macOS.
```

## UI screenshot / SappLink plugin proof

```bash
./build-plugin/SappChoirUiShot_artefacts/Release/SappChoirUiShot.app/Contents/MacOS/SappChoirUiShot /tmp/sappchoir-ui.png
./build-plugin/SappChoirUiShot_artefacts/Release/SappChoirUiShot.app/Contents/MacOS/SappChoirUiShot --cctest
```

## Demo render

```bash
python3 scripts/make_choir_demo.py    # → /tmp/sappchoir-demo.wav
```

## Samples

```bash
~/apps/sappsounds/scripts/fetch-library.sh get sonatina
~/apps/sappsounds/scripts/fetch-library.sh get freepats-synth-choir
~/apps/sappsounds/scripts/fetch-library.sh get legato-vocal
```

No deploy target: this is a local-build instrument. Rollback = git.
