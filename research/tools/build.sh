#!/bin/bash
# build.sh [src dir] [out dir]: every firmware harness and the XtSynth CLI from a
# plugin/Source tree (default the working tree) into out (default $WORK/bin), side by
# side.  What does not build is left missing, not as the last build's binary.
source "$(dirname "$0")/env.sh"
S="${1:-$REPO/plugin/Source}"; O="${2:-$WORK/bin}"; mkdir -p "$O"
# XT_STACK_INIT=pattern (or zero): locals start as that (clang's -ftrivial-auto-var-init), for tone_stack_test.sh (v189)
cc() { "$TC/clang++" -isysroot "$SDK" -std=c++20 -O2 ${XT_STACK_INIT:+-ftrivial-auto-var-init=$XT_STACK_INIT} \
       "${HARNESS_SWITCHES[@]}" "$@" 2>&1 | grep -E " error"; return 0; }
cd "$S" || exit 1
FW=(XtFirmware.cpp $( [ -f XtFwMachine.cpp ] && echo XtFwMachine.cpp ))   # the routines and their machine (v175 on)
SH4=(Sh4Interp.cpp $( [ -f Sh4Setup.cpp ] && echo Sh4Setup.cpp ))   # the interpreter and its setup (v197 on)
# V-Synth's files: patches, a wave's chunks, and the firmware's disk image (its own file from v177 on); the patch store's
# layout and xt_files' checks of patches and of a wave's own chunks (their own files from v187 on)
VF=(XtFiles.cpp XtWaveFile.cpp $( [ -f XtDiskImage.cpp ] && echo XtDiskImage.cpp )
    $( [ -f XtPatchStore.cpp ] && echo XtPatchStore.cpp XtPatchStoreCheck.cpp XtWaveFileCheck.cpp ))
# The arpeggiator in XtSynth (v189): the router and what it sends, the engine (v188), the tempo ticks and a patch's reads
SARP=($( [ -f XtSynthArp.cpp ] && echo XtSynthArp.cpp XtArpeggio.cpp XtArpeggioStep.cpp XtArpeggioMotifs.cpp \
      XtTempoTicks.cpp ))
# XtSynth's parts split out by job and the Multi Step Modulator in it (v193)
SARP+=($( [ -f XtSynthMsm.cpp ] && echo XtSynthZones.cpp XtSynthNotes.cpp XtSynthUpdate.cpp XtSynthKnobs.cpp \
       XtSynthEnvKnobs.cpp XtSynthMsm.cpp XtMsm.cpp XtMsmTrack.cpp ))
# and more of its parts by job (v203): a voice's start, an OSC's, the pad's, a block, the TVA's, the clock's
SARP+=($( [ -f XtSynthBlock.cpp ] && echo XtSynthStart.cpp XtSynthOscStart.cpp XtSynthTrip.cpp XtSynthBlock.cpp \
       XtSynthTva.cpp XtSynthClock.cpp ))
SARP+=($( [ -f XtExtIn.cpp ] && echo XtExtIn.cpp ))   # the EXT IN OSC's player and input (v203)
SARP+=($( [ -f XtSynthOut.cpp ] && echo XtSynthOut.cpp ))   # its output, busOut and render (v204)
SARP+=($( [ -f XtSinc.cpp ] && echo XtSinc.cpp ))   # render's windowed sinc, its own file (v207)
# rom.4's tables, which XtSynthRom.cpp loads (v199 on), and what they read with: vc2_tables.sh's list (v231); the
# descriptors' form, XtPatchStore.cpp, is with each program's own
source "$REPO/research/tools/vc2_tables.sh"
V4=($(vc2Tables . .cpp))
rm -f "$O"/{xt_osc,xt_cosm,xt_lfo,xt_env,xt_fw,xt_enc,xt_analog,xt_synth,xt_tone}
rm -f "$O"/{xt_fx,xt_files,xt_arp,xt_msm,xt_vc2params,xt_vc2in,xt_vc2legacy,xt_vc2hireso,xt_vc2vocoder}
rm -f "$O"/{xt_vc2carrier,xt_vc2tracker,xt_vc2kind6,xt_vc2autonote,xt_vc2mixer,xt_vc2tone,xt_vc2osc,classifier_test,rd}
rm -f "$O"/xt_vc2tva
rm -f "$O"/xt_vc2cosm
cc -DXT_OSC_MAIN XtOsc.cpp XtEnvelope.cpp "${FW[@]}" XtMultiBandPlayer.cpp "${SH4[@]}" -o "$O/xt_osc" &
[ -f XtCosm.cpp ] && cc -DXT_COSM_MAIN XtCosm.cpp XtOsc.cpp XtEnvelope.cpp "${SH4[@]}" -o "$O/xt_cosm" &
cc -DXT_LFO_MAIN XtLfo.cpp "${SH4[@]}" -o "$O/xt_lfo" &
cc -DXT_ENV_MAIN XtEnvelope.cpp "${SH4[@]}" -o "$O/xt_env" &
cc -ffp-contract=off -DXT_FW_MAIN "${FW[@]}" "${SH4[@]}" XtMultiBandPlayer.cpp XtSoloVoice.cpp -o "$O/xt_fw" &
cc -ffp-contract=off -DXT_ENCODER_MAIN XtSoloEncoder.cpp "${FW[@]}" XtMultiBandPlayer.cpp XtSoloVoice.cpp \
   "${SH4[@]}" -o "$O/xt_enc" &
[ -f XtAnalog.cpp ] && cc -DXT_ANALOG_MAIN XtAnalog.cpp XtOsc.cpp XtCosm.cpp XtEnvelope.cpp "${FW[@]}" \
   XtMultiBandPlayer.cpp XtSoloVoice.cpp "${SH4[@]}" -o "$O/xt_analog" &
cc -DXT_SYNTH_MAIN XtSynth.cpp XtSoloEncoder.cpp XtSoloVoice.cpp XtLinearPlayer.cpp XtLitePlayer.cpp XtMultiBandPlayer.cpp XtEnvelope.cpp XtLfo.cpp XtOsc.cpp \
   $( [ -f XtCosm.cpp ] && echo XtCosm.cpp ) $( [ -f XtAnalog.cpp ] && echo XtAnalog.cpp ) "${FW[@]}" \
   "${SH4[@]}" $( [ -f XtSynthRom.cpp ] && echo XtSynthRom.cpp ) "${SARP[@]}" "${V4[@]}" \
   $( [ -f Vc2Params.cpp ] && echo XtPatchStore.cpp ) -o "$O/xt_synth" &
[ -f XtToneCheck.cpp ] && cc -DXT_TONE_MAIN -DXT_TONE_CHECK XtToneCheck.cpp XtSynth.cpp XtSoloEncoder.cpp XtSoloVoice.cpp XtLinearPlayer.cpp XtLitePlayer.cpp XtMultiBandPlayer.cpp XtEnvelope.cpp XtLfo.cpp XtOsc.cpp \
   XtCosm.cpp XtAnalog.cpp "${FW[@]}" "${SH4[@]}" "${VF[@]}" XtPatch.cpp "${SARP[@]}" \
   $( [ -f XtArpeggioPatch.cpp ] && echo XtArpeggioPatch.cpp ) $( [ -f XtSynthMsm.cpp ] && echo XtMsmPatch.cpp ) \
   $( [ -f XtToneMsm.cpp ] && echo XtToneFirmware.cpp XtToneCases.cpp XtToneCarry.cpp XtToneMsm.cpp ) \
   $( [ -f XtTonePatchIds.cpp ] && echo XtTonePatchIds.cpp ) \
   $( [ -f XtSynthRom.cpp ] && echo XtSynthRom.cpp ) "${V4[@]}" -o "$O/xt_tone" &
# rom.4's parameter tables against its own routines (v199), on rom.4
[ -f Vc2ParamsCheck.cpp ] && cc -DXT_VC2PARAMS_MAIN Vc2ParamsCheck.cpp "${V4[@]}" XtPatchStore.cpp "${FW[@]}" \
   XtMultiBandPlayer.cpp XtSoloVoice.cpp "${SH4[@]}" -o "$O/xt_vc2params" &
# rom.4's unvoiced detector (v208) and its consonants (v209) against its own, on rom.4
# (the VC-2 checks' shared compares, v219, with each)
VW=( $( [ -f Vc2CheckWords.cpp ] && echo Vc2CheckWords.cpp ) )
[ -f Vc2UnvoicedCheck.cpp ] && cc -DXT_VC2IN_MAIN Vc2UnvoicedCheck.cpp Vc2Unvoiced.cpp "${VW[@]}" \
   $( [ -f Vc2Consonants.cpp ] && echo Vc2Consonants.cpp ) "${SH4[@]}" -o "$O/xt_vc2in" &
# LEGACY's analysis (v210) and its object (v211) against its own, on rom.4
[ -f Vc2LegacyCheck.cpp ] && cc -DXT_VC2LEGACY_MAIN Vc2LegacyCheck.cpp Vc2LegacyAnalysis.cpp "${VW[@]}" \
   "${SH4[@]}" $( [ -f Vc2Legacy.cpp ] && echo Vc2Legacy.cpp Vc2LegacyTables.cpp Vc2LegacySynthCheck.cpp ) \
   -o "$O/xt_vc2legacy" &
# HIRESO's formant warp (v213), rate filters (v214), analysis (v215) and object (v216) against its own, on rom.4
[ -f Vc2HiresoCheck.cpp ] && cc -DXT_VC2HIRESO_MAIN Vc2HiresoCheck.cpp Vc2HiresoWarp.cpp Vc2HiresoTables.cpp \
   "${VW[@]}" \
   $( [ -f Vc2HiresoRate.cpp ] && echo Vc2HiresoRate.cpp Vc2HiresoRateCheck.cpp ) \
   $( [ -f Vc2HiresoAnalysis.cpp ] && echo Vc2HiresoAnalysis.cpp Vc2HiresoAnalysisCheck.cpp Vc2Unvoiced.cpp ) \
   $( [ -f Vc2Hireso.cpp ] && echo Vc2Hireso.cpp Vc2HiresoObjectCheck.cpp ) \
   $( [ -f Vc2HiresoSynth.cpp ] && echo Vc2HiresoSynth.cpp ) \
   "${SH4[@]}" -o "$O/xt_vc2hireso" &
# The vocoder object (v220) against rom.4's own, with both engines and the stages
VHI=(Vc2HiresoWarp.cpp Vc2HiresoTables.cpp Vc2HiresoRate.cpp Vc2HiresoAnalysis.cpp Vc2Hireso.cpp Vc2HiresoSynth.cpp)
[ -f Vc2VocoderCheck.cpp ] && cc -DXT_VC2VOCODER_MAIN Vc2VocoderCheck.cpp Vc2Vocoder.cpp Vc2VocoderTables.cpp \
   Vc2Unvoiced.cpp Vc2Consonants.cpp Vc2LegacyAnalysis.cpp Vc2Legacy.cpp Vc2LegacyTables.cpp "${VHI[@]}" \
   "${VW[@]}" "${SH4[@]}" -o "$O/xt_vc2vocoder" &
# The VOCODER OSC's carrier (v221), its converter's own part and kind 5's update (v222) against rom.4's own routines
[ -f Vc2CarrierCheck.cpp ] && cc -DXT_VC2CARRIER_MAIN Vc2CarrierCheck.cpp Vc2Carrier.cpp Vc2CarrierTables.cpp \
   $( [ -f Vc2VocoderOsc.cpp ] && echo Vc2VocoderOsc.cpp Vc2VocoderOscCheck.cpp ) \
   $( [ -f Vc2Math.cpp ] && echo Vc2Math.cpp ) "${SH4[@]}" -o "$O/xt_vc2carrier" &
# Voice kind 6, the mic's pitch shifter, and the mic ring (v225) against rom.4's own routines
[ -f Vc2Kind6Check.cpp ] && cc -DXT_VC2KIND6_MAIN Vc2Kind6Check.cpp Vc2Kind6.cpp Vc2Kind6Core.cpp Vc2Kind6Tables.cpp \
   Vc2Math.cpp Vc2TrackerSignal.cpp "${SH4[@]}" -o "$O/xt_vc2kind6" &
# Auto Note and the pitch source (v226), the tracker inside, against rom.4's own routines
[ -f Vc2AutoNoteCheck.cpp ] && cc -DXT_VC2AUTONOTE_MAIN Vc2AutoNoteCheck.cpp Vc2AutoNote.cpp Vc2AutoNoteTables.cpp \
   Vc2Tracker.cpp Vc2TrackerTables.cpp Vc2TrackerAnalysis.cpp Vc2TrackerEstimate.cpp Vc2TrackerSignal.cpp \
   "${SH4[@]}" -o "$O/xt_vc2autonote" &
# The part mixer (v227) against rom.4's own routines, the voices and the calls it makes hooked
[ -f Vc2PartMixerCheck.cpp ] && cc -DXT_VC2MIXER_MAIN Vc2PartMixerCheck.cpp Vc2PartMixerFakes.cpp Vc2PartMixer.cpp \
   Vc2MixerBlock.cpp Vc2PartMixerTables.cpp Vc2Math.cpp "${SH4[@]}" -o "$O/xt_vc2mixer" &
# The tone object's frame (v228) against rom.4's own routines, every part routine hooked
[ -f Vc2ToneCheck.cpp ] && cc -DXT_VC2TONE_MAIN Vc2ToneCheck.cpp Vc2ToneSides.cpp Vc2ToneFakes.cpp Vc2Tone.cpp \
   $( [ -f Vc2ToneNoteOn.cpp ] && echo Vc2ToneNoteOn.cpp Vc2TonePortParts.cpp Vc2ToneEvents.cpp ) \
   "${SH4[@]}" -o "$O/xt_vc2tone" &
# The OSC voice's frame (v230) against rom.4's own routines, every routine it calls hooked; its ramps (v236) its own
[ -f Vc2OscCheck.cpp ] && cc -DXT_VC2OSC_MAIN Vc2OscCheck.cpp Vc2OscEvents.cpp Vc2OscFakes.cpp Vc2OscPortParts.cpp \
   Vc2OscVoice.cpp Vc2OscVoiceGet.cpp Vc2OscVoiceKinds.cpp Vc2OscTables.cpp \
   $( [ -f Vc2OscNoteOn.cpp ] && echo Vc2OscNoteOn.cpp Vc2OscTargets.cpp Vc2OscGetters.cpp ) \
   $( [ -f Vc2PitchRamp.cpp ] && echo Vc2PitchRamp.cpp Vc2PitchRampTick.cpp Vc2RampTables.cpp Vc2RampCheck.cpp ) \
   $( [ -f Vc2OscPlayers.cpp ] && echo Vc2OscPlayers.cpp Vc2OscRecords.cpp ) \
   $( [ -f Vc2OscLite.cpp ] && echo Vc2OscLite.cpp ) \
   $( [ -f Vc2OscAnalog.cpp ] && echo Vc2OscAnalog.cpp Vc2OscAnalogLa.cpp ) \
   $( [ -f Vc2OscAnalogSub.cpp ] && echo Vc2OscAnalogSub.cpp ) \
   $( [ -f Vc2OscAnalogShape.cpp ] && echo Vc2OscAnalogShape.cpp Vc2OscAnalogDraws.cpp ) \
   $( [ -f Vc2OscAnalogUpdate.cpp ] && echo Vc2OscAnalogUpdate.cpp ) \
   "${SH4[@]}" -o "$O/xt_vc2osc" &
# The TVA (v233) against rom.4's own routines, the OS's call hooked
[ -f Vc2TvaCheck.cpp ] && cc -DXT_VC2TVA_MAIN Vc2TvaCheck.cpp Vc2TvaEvents.cpp Vc2TvaFakes.cpp Vc2Tva.cpp \
   Vc2TvaTables.cpp "${SH4[@]}" -o "$O/xt_vc2tva" &
# COSM (v234): the Growl against rom.4's own routines, the Matrix readers hooked; the converters' frame with the TVF's
# converter and the Matrix's readers (v235); the converters by type and v237's six; v239's five; v241's two; the
# Matrix core's part (v243), on the Matrix's own sources (v245); the Matrix set-up's part (v247); the COSM object's
# frame's part (v249); the COSM note-on frame's part (v250), its types adapted over XtCosm (v252)
[ -f Vc2CosmCheck.cpp ] && cc -DXT_VC2COSM_MAIN Vc2CosmCheck.cpp Vc2CosmGrowl.cpp Vc2CosmTables.cpp Vc2Math.cpp \
   $( [ -f Vc2CosmFrameCheck.cpp ] && echo Vc2CosmFrameCheck.cpp Vc2CosmConvert.cpp Vc2CosmTvf.cpp Vc2Matrix.cpp \
   Vc2MatrixTables.cpp ) $( [ -f Vc2CosmConverters.cpp ] && echo Vc2CosmConverters.cpp Vc2CosmFrameDraws.cpp \
   Vc2CosmFilters.cpp Vc2CosmDynamics.cpp ) $( [ -f Vc2CosmDrive.cpp ] && echo Vc2CosmDrive.cpp Vc2CosmBalanced.cpp ) \
   $( [ -f Vc2CosmSideBand.cpp ] && echo Vc2CosmSideBand.cpp ) \
   $( [ -f Vc2MatrixCheck.cpp ] && echo Vc2MatrixCheck.cpp ) \
   $( [ -f Vc2MatrixSources.cpp ] && echo Vc2MatrixSources.cpp ) \
   $( [ -f Vc2MatrixSetup.cpp ] && echo Vc2MatrixSetup.cpp Vc2MatrixSetupCheck.cpp Vc2MatrixSetupDraws.cpp ) \
   $( [ -f Vc2CosmObject.cpp ] && echo Vc2CosmObject.cpp Vc2CosmObjectCheck.cpp ) \
   $( [ -f Vc2CosmNoteOn.cpp ] && echo Vc2CosmPools.cpp Vc2CosmNoteOn.cpp Vc2CosmNoteOnCheck.cpp \
   Vc2CosmNoteOnDraws.cpp ) $( [ -f Vc2CosmAdapters.cpp ] && echo Vc2CosmAdapters.cpp Vc2CosmAdapterWords.cpp \
   Vc2CosmAdapterBlocks.cpp Vc2CosmTbAdapter.cpp Vc2CosmBufferedAdapters.cpp Vc2CosmDsp.cpp Vc2CosmDspCheck.cpp \
   XtCosm.cpp XtOsc.cpp XtEnvelope.cpp ) \
   "${SH4[@]}" -o "$O/xt_vc2cosm" &
# The pitch tracker, its frame (v223) and its analysis (v224), against rom.4's own module
[ -f Vc2TrackerCheck.cpp ] && cc -DXT_VC2TRACKER_MAIN Vc2TrackerCheck.cpp Vc2Tracker.cpp Vc2TrackerTables.cpp \
   $( [ -f Vc2TrackerAnalysis.cpp ] && echo Vc2TrackerAnalysis.cpp Vc2TrackerEstimate.cpp Vc2TrackerSignal.cpp ) \
   "${SH4[@]}" -o "$O/xt_vc2tracker" &
[ -f XtFiles.cpp ] && cc -DXT_FILES_MAIN "${VF[@]}" "${FW[@]}" XtMultiBandPlayer.cpp \
   XtSoloVoice.cpp XtSoloEncoder.cpp "${SH4[@]}" -o "$O/xt_files" &
# xt_fx: the chip and its check, the check's parts in XtFxCheck*.cpp from v204
[ -f XtFxChip.cpp ] && cc -DXT_FX_MAIN -I"$REPO/research/third_party/gearmulator_esp" XtFxChip.cpp XtFxCheck*.cpp \
   XtFxControl.cpp XtFxValueText.cpp XtFxInput.cpp XtExtIn.cpp "${SH4[@]}" -o "$O/xt_fx" &
# The arpeggiator against the firmware's own object (v188), on the disk image's patches; the tempo ticks it steps on (v189)
ARP=(XtArpeggio.cpp XtArpeggioStep.cpp XtArpeggioMotifs.cpp XtArpeggioCheck.cpp XtArpeggioCases.cpp XtTempoTicks.cpp
     XtTempoTicksCheck.cpp XtArpeggioPatch.cpp)
[ -f XtArpeggio.cpp ] && cc -DXT_ARP_MAIN "${ARP[@]}" "${VF[@]}" "${FW[@]}" XtMultiBandPlayer.cpp \
   XtSoloVoice.cpp XtSoloEncoder.cpp "${SH4[@]}" -o "$O/xt_arp" &
# the Multi Step Modulator (v192 on), with the patches' reads for the presets
[ -f XtMsm.cpp ] && cc -DXT_MSM_MAIN XtMsm.cpp XtMsmTrack.cpp XtMsmPatch.cpp XtMsmCheck.cpp XtMsmCases.cpp "${VF[@]}" \
   "${FW[@]}" XtMultiBandPlayer.cpp XtSoloVoice.cpp XtSoloEncoder.cpp "${SH4[@]}" -o "$O/xt_msm" &
# Auto's classifier on a voiced and a pure tone (v191: what the legacy engine's tests kept of it)
CLS=$( [ -f ContentClassifier.cpp ] && [ -f ../Tests/ClassifierTest.cpp ] && echo classifier_test )
[ -n "$CLS" ] && cc ../Tests/ClassifierTest.cpp ContentClassifier.cpp -o "$O/classifier_test" &
cc -I. "$REPO/research/tools/rd.cpp" "${SH4[@]}" -o "$O/rd" &
wait
for b in xt_osc xt_lfo xt_env xt_fw xt_enc xt_synth $CLS rd \
         $( [ -f XtCosm.cpp ] && echo xt_cosm ) $( [ -f XtAnalog.cpp ] && echo xt_analog ) \
         $( [ -f XtFxChip.cpp ] && echo xt_fx ) $( [ -f XtFiles.cpp ] && echo xt_files ) \
         $( [ -f XtToneCheck.cpp ] && echo xt_tone ) $( [ -f XtArpeggio.cpp ] && echo xt_arp ) \
         $( [ -f XtMsm.cpp ] && echo xt_msm ); do
  [ -x "$O/$b" ] || { echo "build: $b missing"; exit 1; }; done
