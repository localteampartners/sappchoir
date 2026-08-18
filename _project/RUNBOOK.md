# RUNBOOK — sappchoir

<!-- UPDATE WHEN: run/deploy/rollback steps change -->

## Build + test (fast loop)

```bash
./verify.sh          # core+CLI+tests AND the plugin + headless harness,
                     # then the unit suite, the headless station regression
                     # and a CLI smoke check
```

## Plugin build

```bash
cmake -S . -B build-plugin -DCMAKE_BUILD_TYPE=Release -DSAPPCHOIR_BUILD_TESTS=OFF \
      -DFETCHCONTENT_SOURCE_DIR_JUCE=$HOME/apps/sappaudio/sappsynth/build/_deps/juce-src
cmake --build build-plugin -j8
# artefacts: build-plugin/SappChoirPlugin_artefacts/Release/{Standalone,VST3,AU}
# AU/VST3 are auto-copied to ~/Library/Audio/Plug-Ins on macOS.
```

## Headless station harness (issue #1)

The station host has no editor and never pumps a JUCE dispatch loop. This
harness is that host — it is how a silent-render report gets diagnosed.

```bash
H=./build-plugin/SappChoirHeadless_artefacts/Release/sappchoir-headless
$H selftest                                   # the regression (14 checks)
$H render --root ~/Samples --settle 8000 \
   --instrument "vpo/Virtual-Playing-Orchestra3/Vocals/choir-MIXED-sustain" \
   --out /tmp/choir.wav                       # prints voices / RMS / peak / dBFS
$H render ... --pump                          # pretend to be a JUCE host
```

A host should poll the read-only `libraryReady` parameter rather than guess
a settle window. When a render sounds wrong, grep the host log (or
`$SAPP_CHOIR_LOG`) for `SappChoir-audio-source:` — it names the SFZ that
actually produced the audio, and says `DIAGNOSTIC(...)` when the built-in
choir is what sounded.

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

## Release rule (in-plugin updater)

Bump `project(SappChoir VERSION X.Y.Z)` in CMakeLists.txt to match every
release tag — the in-plugin updater compares JucePlugin_VersionString
against the latest GitHub tag, so the two MUST stay in sync.
