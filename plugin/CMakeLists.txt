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
    # The Standalone's audio input, which the EXT IN OSCs play (v207): macOS asks once for the microphone
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "V-Synth Emu's EXT IN oscillators play the audio input, as the V-Synth XT's do."

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
    Source/PluginStartOffset.cpp
    Source/PluginWaveEdit.cpp
    Source/PluginWaveCut.cpp
    Source/PluginWaveEvents.cpp
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
    Source/PluginPresetWaves.cpp
    Source/PluginProject.cpp
    Source/PluginPatchSave.cpp
    Source/PluginProjectSave.cpp
    Source/PluginEncodes.cpp
    Source/PluginEffects.cpp
    Source/PluginState.cpp
    Source/PluginWaveView.cpp
    Source/PluginProcess.cpp
    Source/UiLook.cpp
    Source/UiKnob.cpp
    Source/UiHeader.cpp
    Source/UiPatchBox.cpp
    Source/UiPatchSave.cpp
    Source/UiPageBar.cpp
    Source/UiPath.cpp
    Source/UiFooter.cpp
    Source/UiEditor.cpp
    Source/UiControls.cpp
    Source/UiFacedPanel.cpp
    Source/UiOscPanel.cpp
    Source/UiCosmPanel.cpp
    Source/UiTripColumn.cpp
    Source/UiRack.cpp
    Source/UiTabs.cpp
    Source/UiBlockKinds.cpp
    Source/UiDetailTables.cpp
    Source/UiWatch.cpp
    Source/UiFields.cpp
    Source/UiEnvelope.cpp
    Source/UiLfo.cpp
    Source/UiRoutes.cpp
    Source/UiDetails.cpp
    Source/UiOverviews.cpp
    Source/UiLowerPanel.cpp
    Source/UiControlGrid.cpp
    Source/UiCard.cpp
    Source/UiPages.cpp
    Source/UiMsm.cpp
    Source/UiBars.cpp
    Source/UiMatrix.cpp
    Source/UiModPage.cpp
    Source/UiArpPattern.cpp
    Source/UiPlayPage.cpp
    Source/UiZoneStrip.cpp
    Source/UiZonesPage.cpp
    Source/UiWaveDisplay.cpp
    Source/UiWaveEdit.cpp
    Source/UiWaveEditPaint.cpp
    Source/UiWaveEditEvents.cpp
    Source/UiWavePage.cpp
    Source/UiTripPad.cpp
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
    Source/XtSynthStart.cpp
    Source/XtSynthOscStart.cpp
    Source/XtSynthTrip.cpp
    Source/XtSynthBlock.cpp
    Source/XtSynthTva.cpp
    Source/XtSynthClock.cpp
    Source/XtSynthOut.cpp
    Source/XtSynthImport.cpp
    Source/XtSinc.cpp
    Source/XtRateOut.cpp
    Source/XtExtIn.cpp
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
    Source/Sh4Setup.cpp
    Source/PluginRom.cpp
    Source/PluginAppFolder.cpp
    Source/XtFirmware.cpp
    Source/XtFwEncode.cpp
    Source/XtFwEvents.cpp
    Source/XtFwMachine.cpp
    Source/XtFiles.cpp
    Source/XtPatchStore.cpp
    Source/XtWaveFile.cpp
    Source/XtDiskImage.cpp
    Source/XtPresetWaves.cpp
    Source/XtFwPatchFile.cpp
    Source/XtFwFileOut.cpp
    Source/XtFwWaveFile.cpp
    Source/XtPatchIds.cpp
    Source/XtPatch.cpp
    Source/XtFxChip.cpp
    Source/XtFxControl.cpp
    Source/XtFxUnit.cpp
    Source/XtFxImage.cpp
    Source/XtFxInput.cpp
    Source/PluginExtIn.cpp
    Source/PluginVc2Params.cpp
    Source/PluginVc2Patch.cpp
    Source/PluginVc2Play.cpp
    Source/PluginVc2Fx.cpp
    Source/PluginRender.cpp
    Source/XtHostInput.cpp
    Source/XtFxValueText.cpp
    Source/XtBuildId.cpp
)
include(Vc2Sources.cmake)   # the VC-2 mode's ports, rom.4's (v251: moved out of this list as they stood)
include(Vc2UiSources.cmake)   # the VC-2 mode's pages in the editor (v453)

# ─── Which build this is ─────────────────────────────────────────────────────
# Worked out at every build, not at configure, so it can't go stale.  It rewrites the file only when the id changes.
set(XT_BUILD_ID_H "${CMAKE_CURRENT_BINARY_DIR}/generated/XtBuildIdGenerated.h")
add_custom_target(VSynthEmuBuildId
    COMMAND "${CMAKE_COMMAND}" "-DREPO=${CMAKE_CURRENT_SOURCE_DIR}/.." "-DOUT=${XT_BUILD_ID_H}" -P "${CMAKE_CURRENT_SOURCE_DIR}/BuildId.cmake"
    BYPRODUCTS "${XT_BUILD_ID_H}"
    VERBATIM)
set_source_files_properties(Source/XtBuildId.cpp PROPERTIES INCLUDE_DIRECTORIES "${CMAKE_CURRENT_BINARY_DIR}/generated")

# ─── JUCE Modules ────────────────────────────────────────────────────────────
# A plugin target's set-up past its sources (v462: the instrument's and the effect build's, FxPlugin.cmake): the build
# id first, the JUCE modules, the definitions
function(vse_plugin_setup target)
    add_dependencies(${target} VSynthEmuBuildId)
    target_link_libraries(${target} PRIVATE
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
    target_compile_definitions(${target} PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
    )
endfunction()
vse_plugin_setup(VSynthEmu)
include(FxPlugin.cmake)   # the effect build (v462), on the sources above

# ─── Tests of the processor (not built by default) ───────────────────────────
# A test linked to the plugin's shared code, with JUCE's headers and settings as that code sees them
function(vse_add_test name)
    add_executable(${name} EXCLUDE_FROM_ALL ${ARGN})
    target_link_libraries(${name} PRIVATE VSynthEmu)
    target_include_directories(${name} PRIVATE $<TARGET_PROPERTY:VSynthEmu,INCLUDE_DIRECTORIES>)
    target_compile_definitions(${name} PRIVATE $<TARGET_PROPERTY:VSynthEmu,COMPILE_DEFINITIONS>)
endfunction()
vse_add_test(VSynthEmuTest Tests/PluginTest.cpp)   # the XT path end to end
vse_add_test(VSynthEmuTeardownTest Tests/TeardownTest.cpp Tests/TestMain.cpp Tests/TestWave.cpp)   # closed mid-encode
vse_add_test(VSynthEmuWaveFileTest Tests/WaveFileTest.cpp)     # a wave whose header claims more than its data holds
vse_add_test(VSynthEmuRestoreTest Tests/RestoreTest.cpp Tests/TestMain.cpp Tests/TestWave.cpp)   # reopened, parts gone
vse_add_test(VSynthEmuRomTest Tests/RomTest.cpp Tests/RomWavesTest.cpp Tests/TestMain.cpp Tests/TestNote.cpp)   # another image while one plays; the VC-2's waves at the start (v504)
# The harnesses' switches set in the plugin's environment, which it doesn't read
vse_add_test(VSynthEmuSwitchTest Tests/SwitchTest.cpp Tests/TestMain.cpp Tests/TestNote.cpp Tests/TestWave.cpp)
# A firmware routine that loops, on each way the plugin runs one: it gives up within its step limit
vse_add_test(VSynthEmuStepTest Tests/StepTest.cpp Tests/TestMain.cpp Tests/TestNote.cpp Tests/TestWave.cpp)
# The firmware's disk image reader on damaged images: refused, or each cluster once
vse_add_test(VSynthEmuDiskImageTest Tests/DiskImageTest.cpp Tests/TestMain.cpp)
# XtSynth's arpeggiator heard: a note a quarter at two tempos, the Switch off, the Hold pedal (v189)
vse_add_test(VSynthEmuArpTest Tests/ArpTest.cpp Tests/TestMain.cpp)
# The arpeggio's plugin side: its parameters, a patch's arpeggio, a session's, and a chord through the processor (v190);
# then the Multi Step Modulator's: its parameters, the image's patches with it on, a note, a patch's load stopping it (v193);
# the VC-2's parameters against rom.4 (v451), and a .p02 onto them and the session (v452)
vse_add_test(VSynthEmuArpPluginTest Tests/ArpPluginTest.cpp Tests/MsmPluginTest.cpp Tests/ZonePluginTest.cpp
             Tests/Vc2ParamPluginTest.cpp Tests/Vc2PatchPluginTest.cpp Tests/StartOffsetPluginTest.cpp Tests/TestMain.cpp)
# The editor, D (research/UI_DESIGN.md; v196): its layout, every control on its parameter, every press's route
vse_add_test(VSynthEmuEditorTest Tests/EditorTest.cpp Tests/EditorRackTest.cpp Tests/EditorLowerTest.cpp
             Tests/EditorPagesTest.cpp Tests/EditorWaveTest.cpp Tests/EditorWavePageTest.cpp Tests/EditorWaveCutTest.cpp
             Tests/EditorWaveEventTest.cpp Tests/EditorKeysTest.cpp
             Tests/EditorPadTest.cpp
             Tests/EditorVc2Test.cpp Tests/EditorVc2TextTest.cpp Tests/EditorPatchBoxTest.cpp Tests/TestMain.cpp
             Tests/TestWave.cpp)
# The outside input from the plugin's input bus to the EXT IN OSC (v207): the feed alone at each rate, the processor
# against XtSynth given the input by the law, the Effects off and on
vse_add_test(VSynthEmuExtInTest Tests/ExtInTest.cpp Tests/ExtInPluginTest.cpp Tests/TestMain.cpp)
# The output always numbers (v454): a float wave holding NaN and infinities, played with no image, comes out finite
vse_add_test(VSynthEmuOutputTest Tests/OutputTest.cpp Tests/TestMain.cpp Tests/TestWave.cpp)
# The VC-2 mode's audio (v455): the processor against the part driven directly, bit for bit, nothing allocated; the
# part's view (v456) and the system's parameters (v457)
vse_add_test(VSynthEmuVc2PlayTest Tests/Vc2PlayTest.cpp Tests/Vc2PlayRun.cpp Tests/Vc2PlayTime.cpp Tests/Vc2PlayView.cpp
             Tests/Vc2PlaySystem.cpp Tests/Vc2PlayFx.cpp Tests/Vc2PlayRender.cpp Tests/TestMain.cpp Tests/TestWave.cpp)
# A project's own files (v600 on): a link to a preset wave followed to the wave it names
vse_add_test(VSynthEmuProjectTest Tests/ProjectTest.cpp Tests/ProjectSaveTest.cpp Tests/ProjectSaveAsTest.cpp
             Tests/TestMain.cpp Tests/TestWave.cpp)
# What verify.sh builds, in one make: the plugin's links and the tests' run side by side
add_custom_target(VSynthEmuVerify)
add_dependencies(VSynthEmuVerify VSynthEmu_All VSynthEmuFx_All VSynthEmuTest VSynthEmuTeardownTest VSynthEmuWaveFileTest
                 VSynthEmuRestoreTest VSynthEmuRomTest VSynthEmuSwitchTest VSynthEmuStepTest
                 VSynthEmuDiskImageTest VSynthEmuArpTest VSynthEmuArpPluginTest VSynthEmuEditorTest
                 VSynthEmuExtInTest VSynthEmuOutputTest VSynthEmuProjectTest VSynthEmuVc2PlayTest)
