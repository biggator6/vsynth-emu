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
# rom.4's parameters, which XtSynthRom.cpp loads (v199), and what they read with: the values' text, the descriptors' form;
# and the unvoiced detector's coefficients (v208), LEGACY's tables (v210; their own file from v211) and HIRESO's (v213),
# which it loads too
V4=($( [ -f Vc2Params.cpp ] && echo Vc2Params.cpp XtFxValueText.cpp )
    $( [ -f Vc2Unvoiced.cpp ] && echo Vc2Unvoiced.cpp ) $( [ -f Vc2LegacyAnalysis.cpp ] && echo Vc2LegacyAnalysis.cpp )
    $( [ -f Vc2LegacyTables.cpp ] && echo Vc2LegacyTables.cpp )
    $( [ -f Vc2HiresoTables.cpp ] && echo Vc2HiresoTables.cpp ))
rm -f "$O"/{xt_osc,xt_cosm,xt_lfo,xt_env,xt_fw,xt_enc,xt_analog,xt_synth,xt_tone}
rm -f "$O"/{xt_fx,xt_files,xt_arp,xt_msm,xt_vc2params,xt_vc2in,xt_vc2legacy,xt_vc2hireso,classifier_test,rd}
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
