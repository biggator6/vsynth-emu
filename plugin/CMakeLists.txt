cmake_minimum_required(VERSION 3.22)
project(VSynthEmu VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ─── JUCE ────────────────────────────────────────────────────────────────────
# JUCE 8.0.7 (10a5896) is the submodule at ./JUCE (.gitmodules). In a fresh clone:
#   git submodule update --init JUCE
# Or point JUCE_PATH at another checkout of it.

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
    COMPANY_NAME            "Gato Gordo Labs"
    # JUCE's default id is com.<company>.<target>, which would hold the name's spaces
    BUNDLE_ID               com.GatoGordoLabs.VSynthEmu
    # The AU's identity (aumu VsEm Gato) and the VST3's class id, which JUCE builds from these two codes: a new code is a
    # new plugin to every host, so sessions saved with the old one don't find it (Ystu until v186, the user's choice).
    PLUGIN_MANUFACTURER_CODE Gato
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
    Source/PluginParams.cpp
    Source/PluginParamsOsc.cpp
    Source/PluginParamsCosm.cpp
    Source/PluginParamsTone.cpp
    Source/PluginControlsRead.cpp
    Source/PluginControlsWrite.cpp
    Source/PluginCosmControls.cpp
    Source/PluginOsc2Controls.cpp
    Source/PluginArp.cpp
    Source/PluginSetting.cpp
    Source/PluginMsm.cpp
    Source/PluginZones.cpp
    Source/PluginPatchFiles.cpp
    Source/PluginEncodes.cpp
    Source/PluginEffects.cpp
    Source/PluginState.cpp
    Source/PluginEditor.cpp
    Source/PluginEditorPlay.cpp
    Source/PluginEditorTabs.cpp
    Source/PluginEditorZones.cpp
    Source/PluginEditorMsm.cpp
    Source/PluginEditorMatrix.cpp
    Source/FxPanel.cpp
    Source/ContentClassifier.cpp
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
    Source/XtSynthArp.cpp
    Source/XtSynthZones.cpp
    Source/XtSynthNotes.cpp
    Source/XtSynthUpdate.cpp
    Source/XtSynthKnobs.cpp
    Source/XtSynthEnvKnobs.cpp
    Source/XtSynthMsm.cpp
    Source/XtMsm.cpp
    Source/XtMsmTrack.cpp
    Source/XtMsmPatch.cpp
    Source/XtTempoTicks.cpp
    Source/XtArpeggio.cpp
    Source/XtArpeggioStep.cpp
    Source/XtArpeggioMotifs.cpp
    Source/XtArpeggioPatch.cpp
    Source/XtSynthRom.cpp
    Source/XtMultiBandPlayer.cpp
    Source/Sh4Interp.cpp
    Source/XtFirmware.cpp
    Source/XtFwMachine.cpp
    Source/XtFiles.cpp
    Source/XtPatchStore.cpp
    Source/XtWaveFile.cpp
    Source/XtDiskImage.cpp
    Source/XtPatch.cpp
    Source/XtFxChip.cpp
    Source/XtFxControl.cpp
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
vse_add_test(VSynthEmuTest Tests/PluginTest.cpp Tests/LayoutTest.cpp)   # the XT path end to end; the layout (v193)
vse_add_test(VSynthEmuTeardownTest Tests/TeardownTest.cpp Tests/TestMain.cpp Tests/TestWave.cpp)   # closed mid-encode
vse_add_test(VSynthEmuWaveFileTest Tests/WaveFileTest.cpp)     # a wave whose header claims more than its data holds
vse_add_test(VSynthEmuRestoreTest Tests/RestoreTest.cpp Tests/TestMain.cpp Tests/TestWave.cpp)   # reopened, parts gone
vse_add_test(VSynthEmuRomTest Tests/RomTest.cpp Tests/TestMain.cpp Tests/TestNote.cpp)   # another image while one plays
# The harnesses' switches set in the plugin's environment, which it doesn't read
vse_add_test(VSynthEmuSwitchTest Tests/SwitchTest.cpp Tests/TestMain.cpp Tests/TestNote.cpp Tests/TestWave.cpp)
# A firmware routine that loops, on each way the plugin runs one: it gives up within its step limit
vse_add_test(VSynthEmuStepTest Tests/StepTest.cpp Tests/TestMain.cpp Tests/TestNote.cpp Tests/TestWave.cpp)
# The firmware's disk image reader on damaged images: refused, or each cluster once
vse_add_test(VSynthEmuDiskImageTest Tests/DiskImageTest.cpp Tests/TestMain.cpp)
# XtSynth's arpeggiator heard: a note a quarter at two tempos, the Switch off, the Hold pedal (v189)
vse_add_test(VSynthEmuArpTest Tests/ArpTest.cpp Tests/TestMain.cpp)
# The arpeggio's plugin side: its parameters, a patch's arpeggio, a session's, and a chord through the processor (v190);
# then the Multi Step Modulator's: its parameters, the image's patches with it on, a note, a patch's load stopping it (v193)
vse_add_test(VSynthEmuArpPluginTest Tests/ArpPluginTest.cpp Tests/MsmPluginTest.cpp Tests/ZonePluginTest.cpp
             Tests/TestMain.cpp)
# What verify.sh builds, in one make: the plugin's links and the tests' run side by side
add_custom_target(VSynthEmuVerify)
add_dependencies(VSynthEmuVerify VSynthEmu_All VSynthEmuTest VSynthEmuTeardownTest VSynthEmuWaveFileTest
                 VSynthEmuRestoreTest VSynthEmuRomTest VSynthEmuSwitchTest VSynthEmuStepTest
                 VSynthEmuDiskImageTest VSynthEmuArpTest VSynthEmuArpPluginTest)
