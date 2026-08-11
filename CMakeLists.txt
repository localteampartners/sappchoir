cmake_minimum_required(VERSION 3.24)

project(SappChoir VERSION 0.8.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(SAPPCHOIR_BUILD_PLUGIN "Build JUCE plugin + standalone targets" ON)
option(SAPPCHOIR_BUILD_TESTS  "Build unit tests"                       ON)
option(SAPPCHOIR_BUILD_CLI    "Build the sappchoir agent CLI"          ON)

include(cmake/CompilerWarnings.cmake)
include(FetchContent)

# ------------------------------------------------------------- SappSounds ---
# Local development uses the sibling checkout; CI or isolated builds may set
# SAPPSOUNDS_DIR or fall back to FetchContent from the public repo.
set(SAPPSOUNDS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../sappsounds" CACHE PATH
    "Path to a local SappSounds checkout (sibling directory by default)")

if(EXISTS "${SAPPSOUNDS_DIR}/CMakeLists.txt")
  add_subdirectory("${SAPPSOUNDS_DIR}" "${CMAKE_BINARY_DIR}/SappSounds")
else()
  FetchContent_Declare(SappSounds
    GIT_REPOSITORY https://github.com/localteampartners/sappsounds.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
  )
  FetchContent_MakeAvailable(SappSounds)
endif()

# ------------------------------------------------------------------- core ---
# Choir policy on top of Sapp::Sounds. Framework-independent (no JUCE):
# consumed by the plugin, the CLI, and the tests.
add_library(sappchoir_core STATIC
  src/core/ChoirEngine.cpp
  src/core/VowelLayers.cpp
  src/core/ChoirRender.cpp
  src/core/SappLinkCCMap.cpp
  src/core/SfzLibrary.cpp
)
target_include_directories(sappchoir_core PUBLIC src)
target_link_libraries(sappchoir_core PUBLIC Sapp::Sounds)
# The build stamp the diagnostic log lines carry (SappChoir-build:, and
# SappChoir-audio-source: when a voice batch starts from silence).
target_compile_definitions(sappchoir_core PUBLIC SAPPCHOIR_VERSION="${PROJECT_VERSION}")
sappchoir_set_warnings(sappchoir_core)

# -------------------------------------------------------------------- cli ---
if(SAPPCHOIR_BUILD_CLI)
  add_executable(sappchoir-cli src/cli/main.cpp)
  set_target_properties(sappchoir-cli PROPERTIES OUTPUT_NAME sappchoir)
  target_link_libraries(sappchoir-cli PRIVATE sappchoir_core)
  sappchoir_set_warnings(sappchoir-cli)
endif()

# ------------------------------------------------------------------ tests ---
if(SAPPCHOIR_BUILD_TESTS)
  FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
    GIT_SHALLOW    TRUE
  )
  FetchContent_MakeAvailable(Catch2)

  add_executable(SappChoirTests
    tests/unit/test_cathedral.cpp
    tests/unit/test_vowel.cpp
    tests/unit/test_choir_engine.cpp
    tests/unit/test_render.cpp
    tests/unit/test_sapplink.cpp
    tests/unit/test_sfz_library.cpp
  )
  target_link_libraries(SappChoirTests PRIVATE sappchoir_core Catch2::Catch2WithMain)
  target_compile_definitions(SappChoirTests PRIVATE
    SAPPCHOIR_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data")
  sappchoir_set_warnings(SappChoirTests)

  enable_testing()
  add_test(NAME unit COMMAND SappChoirTests)
endif()

# ----------------------------------------------------------------- plugin ---
if(SAPPCHOIR_BUILD_PLUGIN)
  # Same pinned JUCE tag as SappSynth / SappOrchestra.
  FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.15
    GIT_SHALLOW    TRUE
  )
  FetchContent_MakeAvailable(JUCE)

  juce_add_plugin(SappChoirPlugin
    PRODUCT_NAME                "SappChoir"
    COMPANY_NAME                "LocalTeamPartners"
    BUNDLE_ID                   com.localteampartners.sappchoir
    PLUGIN_MANUFACTURER_CODE    Ltpr
    PLUGIN_CODE                 Schr
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    FORMATS                     Standalone VST3 AU
  )
  if(APPLE)
    juce_enable_copy_plugin_step(SappChoirPlugin)
  endif()

  target_sources(SappChoirPlugin PRIVATE
    src/plugin/PluginProcessor.cpp
    src/plugin/PluginEditor.cpp
    src/plugin/SoundsPanel.cpp
  )
  target_link_libraries(SappChoirPlugin PRIVATE
    sappchoir_core
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
  )
  target_compile_definitions(SappChoirPlugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
  )

  # ---------------------------------------------------- headless harness ---
  # The station host: no editor, no dispatch loop. Cross-platform on purpose —
  # sappchoir #1 only reproduces where the JUCE message loop never runs, so
  # this is the target that guards it (and CTest runs its selftest).
  juce_add_console_app(SappChoirHeadless PRODUCT_NAME "sappchoir-headless")
  target_sources(SappChoirHeadless PRIVATE
    tools/headless/main.cpp
    src/plugin/PluginProcessor.cpp
    src/plugin/PluginEditor.cpp
    src/plugin/SoundsPanel.cpp
  )
  target_include_directories(SappChoirHeadless PRIVATE src/plugin)
  target_link_libraries(SappChoirHeadless PRIVATE
    sappchoir_core
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
  )
  target_compile_definitions(SappChoirHeadless PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1
    SAPPCHOIR_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data")
  set_target_properties(SappChoirHeadless PROPERTIES OUTPUT_NAME sappchoir-headless)

  if(SAPPCHOIR_BUILD_TESTS)
    # The regression that guards issue #1. It runs the PLUGIN processor, so it
    # only exists when the plugin target is built — which is the point
    # (sappkeys #1: tests green while the installed binary stayed stale).
    add_test(NAME headless COMMAND SappChoirHeadless selftest)
  endif()

  if(APPLE)
    # Offscreen editor snapshot tool (UI verification without screen capture).
    juce_add_gui_app(SappChoirUiShot PRODUCT_NAME "SappChoirUiShot")
    target_sources(SappChoirUiShot PRIVATE
      tools/uishot/main.cpp
      src/plugin/PluginProcessor.cpp
      src/plugin/PluginEditor.cpp
      src/plugin/SoundsPanel.cpp
    )
    target_include_directories(SappChoirUiShot PRIVATE src/plugin)
    target_link_libraries(SappChoirUiShot PRIVATE
      sappchoir_core
      juce::juce_audio_utils
      juce::juce_dsp
      juce::juce_recommended_config_flags
    )
    target_compile_definitions(SappChoirUiShot PRIVATE
      JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
      JUCE_MODAL_LOOPS_PERMITTED=1)  # --sfztest pumps the dispatch loop
  endif()
endif()
