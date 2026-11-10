cmake_minimum_required(VERSION 3.22)
project(VSynthEmu VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ─── JUCE ────────────────────────────────────────────────────────────────────
# Assumes JUCE is either a sibling directory or installed via FetchContent.
# Option 1 (recommended for dev): clone JUCE alongside this project
#   git clone https://github.com/juce-framework/JUCE.git ../JUCE
# Option 2: FetchContent (uncomment below)

# include(FetchContent)
# FetchContent_Declare(JUCE
#     GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
#     GIT_TAG        7.0.9
# )
# FetchContent_MakeAvailable(JUCE)

# Local JUCE path (adjust if needed)
set(JUCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE"
    CACHE PATH "Path to JUCE framework")

if(NOT EXISTS "${JUCE_PATH}/CMakeLists.txt")
    message(FATAL_ERROR
        "JUCE not found at ${JUCE_PATH}. "
        "Clone JUCE alongside this directory or set JUCE_PATH.")
endif()

add_subdirectory(${JUCE_PATH} JUCE)

# ─── Plugin Target ───────────────────────────────────────────────────────────
juce_add_plugin(VSynthEmu
    COMPANY_NAME            "YourStudio"
    PLUGIN_MANUFACTURER_CODE Ystu
    PLUGIN_CODE             VsEm
    FORMATS                 VST3 AU Standalone
    PRODUCT_NAME            "V-Synth Emu"

    IS_SYNTH                TRUE
    NEEDS_MIDI_INPUT        TRUE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE

    COPY_PLUGIN_AFTER_BUILD TRUE
    # The manifest helper cannot load the ad-hoc-signed bundle on macOS 27
    # ("couldn't be loaded", though dlopen works); moduleinfo.json is optional.
    VST3_AUTO_MANIFEST      FALSE
)

# ─── Sources ─────────────────────────────────────────────────────────────────
target_sources(VSynthEmu PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEncodes.cpp
    Source/PluginEffects.cpp
    Source/PluginState.cpp
    Source/PluginEditor.cpp
    Source/FxPanel.cpp
    Source/VariphraseEngine.cpp
    Source/PhaseVocoder.cpp
    Source/SourceFilterModel.cpp
    Source/XtSoloEncoder.cpp
    Source/XtEnvelope.cpp
    Source/XtLfo.cpp
    Source/XtOsc.cpp
    Source/XtCosm.cpp
    Source/XtAnalog.cpp
    Source/XtSoloVoice.cpp
    Source/XtLinearPlayer.cpp
    Source/XtLitePlayer.cpp
    Source/XtSynth.cpp
    Source/XtSynthRom.cpp
    Source/XtMultiBandPlayer.cpp
    Source/Sh4Interp.cpp
    Source/XtFirmware.cpp
    Source/XtFiles.cpp
    Source/XtWaveFile.cpp
    Source/XtPatch.cpp
    Source/XtFxChip.cpp
    Source/XtFxControl.cpp
    Source/OfflineRenderer.cpp
    Source/XtBuildId.cpp
)

# ─── Which build this is ─────────────────────────────────────────────────────
# Worked out at every build, not at configure, so it can't go stale.  It rewrites the file only when the id changes.
set(XT_BUILD_ID_H "${CMAKE_CURRENT_BINARY_DIR}/generated/XtBuildIdGenerated.h")
add_custom_target(VSynthEmuBuildId
    COMMAND "${CMAKE_COMMAND}" "-DREPO=${CMAKE_CURRENT_SOURCE_DIR}/.." "-DOUT=${XT_BUILD_ID_H}" -P "${CMAKE_CURRENT_SOURCE_DIR}/BuildId.cmake"
    BYPRODUCTS "${XT_BUILD_ID_H}"
    VERBATIM)
add_dependencies(VSynthEmu VSynthEmuBuildId)
set_source_files_properties(Source/XtBuildId.cpp PROPERTIES INCLUDE_DIRECTORIES "${CMAKE_CURRENT_BINARY_DIR}/generated")

# ─── JUCE Modules ────────────────────────────────────────────────────────────
target_link_libraries(VSynthEmu PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_audio_basics
    juce::juce_audio_formats
    juce::juce_dsp
    juce::juce_gui_basics
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

target_compile_definitions(VSynthEmu PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

# ─── Tests of the processor (not built by default) ───────────────────────────
# A test linked to the plugin's shared code, with JUCE's headers and settings as that code sees them
function(vse_add_test name)
    add_executable(${name} EXCLUDE_FROM_ALL ${ARGN})
    target_link_libraries(${name} PRIVATE VSynthEmu)
    target_include_directories(${name} PRIVATE $<TARGET_PROPERTY:VSynthEmu,INCLUDE_DIRECTORIES>)
    target_compile_definitions(${name} PRIVATE $<TARGET_PROPERTY:VSynthEmu,COMPILE_DEFINITIONS>)
endfunction()
vse_add_test(VSynthEmuTest Tests/PluginTest.cpp)               # the XT path end to end
vse_add_test(VSynthEmuTeardownTest Tests/TeardownTest.cpp Tests/TestMain.cpp Tests/TestWave.cpp)   # closed mid-encode
vse_add_test(VSynthEmuWaveFileTest Tests/WaveFileTest.cpp)     # a wave whose header claims more than its data holds
vse_add_test(VSynthEmuRestoreTest Tests/RestoreTest.cpp Tests/TestMain.cpp Tests/TestWave.cpp)   # reopened, parts gone
vse_add_test(VSynthEmuRomTest Tests/RomTest.cpp Tests/TestMain.cpp Tests/TestNote.cpp)   # another image while one plays
# The harnesses' switches set in the plugin's environment, which it doesn't read
vse_add_test(VSynthEmuSwitchTest Tests/SwitchTest.cpp Tests/TestMain.cpp Tests/TestNote.cpp Tests/TestWave.cpp)
# What verify.sh builds, in one make: the plugin's links and the tests' run side by side
add_custom_target(VSynthEmuVerify)
add_dependencies(VSynthEmuVerify VSynthEmu_All VSynthEmuTest VSynthEmuTeardownTest VSynthEmuWaveFileTest
                 VSynthEmuRestoreTest VSynthEmuRomTest VSynthEmuSwitchTest)

# ─── Optional: Python bindings for testing (pybind11) ────────────────────────
# Uncomment when you want to call VariphraseEngine from Python for batch tests.
# find_package(pybind11 REQUIRED)
# pybind11_add_module(variphrase_py Source/VariphraseEngine.cpp Source/PhaseVocoder.cpp
#                                   Source/pybind11_bindings.cpp)
# target_link_libraries(variphrase_py PRIVATE juce::juce_audio_basics juce::juce_dsp)
