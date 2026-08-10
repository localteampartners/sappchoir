# ENVIRONMENT — sappchoir

<!-- UPDATE WHEN: an env var is added/renamed/removed -->

No .env, no secrets. Build-time knobs are CMake options
(`SAPPCHOIR_BUILD_PLUGIN/TESTS/CLI`, `SAPPSOUNDS_DIR`,
`FETCHCONTENT_SOURCE_DIR_JUCE`).

Two optional runtime variables, both read by the plugin and the CLI:

| Var | Effect |
|---|---|
| `SAPP_SFZ_ROOT` | Overrides the samples root the `instrument` choice list is scanned from (wins over the shared Sapp `samplesRoot` setting). Used by headless tests and driving sessions. |
| `SAPP_CHOIR_LOG` | When it names a file, the `SappChoir-build:` / `SappChoir-instrument:` / `SappChoir-audio-source:` diagnostic lines are appended to it as well as going to the host's JUCE logger (and stderr on Windows). |
