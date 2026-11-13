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
rm -f "$O"/{xt_osc,xt_cosm,xt_lfo,xt_env,xt_fw,xt_enc,xt_analog,xt_synth,xt_tone}
rm -f "$O"/{xt_fx,xt_files,xt_arp,xt_msm,classifier_test,rd}
cc -DXT_OSC_MAIN XtOsc.cpp XtEnvelope.cpp "${FW[@]}" XtMultiBandPlayer.cpp Sh4Interp.cpp -o "$O/xt_osc" &
[ -f XtCosm.cpp ] && cc -DXT_COSM_MAIN XtCosm.cpp XtOsc.cpp XtEnvelope.cpp Sh4Interp.cpp -o "$O/xt_cosm" &
cc -DXT_LFO_MAIN XtLfo.cpp Sh4Interp.cpp -o "$O/xt_lfo" &
cc -DXT_ENV_MAIN XtEnvelope.cpp Sh4Interp.cpp -o "$O/xt_env" &
cc -ffp-contract=off -DXT_FW_MAIN "${FW[@]}" Sh4Interp.cpp XtMultiBandPlayer.cpp XtSoloVoice.cpp -o "$O/xt_fw" &
cc -ffp-contract=off -DXT_ENCODER_MAIN XtSoloEncoder.cpp "${FW[@]}" XtMultiBandPlayer.cpp XtSoloVoice.cpp \
   Sh4Interp.cpp -o "$O/xt_enc" &
[ -f XtAnalog.cpp ] && cc -DXT_ANALOG_MAIN XtAnalog.cpp XtOsc.cpp XtCosm.cpp XtEnvelope.cpp "${FW[@]}" \
   XtMultiBandPlayer.cpp XtSoloVoice.cpp Sh4Interp.cpp -o "$O/xt_analog" &
cc -DXT_SYNTH_MAIN XtSynth.cpp XtSoloEncoder.cpp XtSoloVoice.cpp XtLinearPlayer.cpp XtLitePlayer.cpp XtMultiBandPlayer.cpp XtEnvelope.cpp XtLfo.cpp XtOsc.cpp \
   $( [ -f XtCosm.cpp ] && echo XtCosm.cpp ) $( [ -f XtAnalog.cpp ] && echo XtAnalog.cpp ) "${FW[@]}" \
   Sh4Interp.cpp $( [ -f XtSynthRom.cpp ] && echo XtSynthRom.cpp ) "${SARP[@]}" -o "$O/xt_synth" &
[ -f XtToneCheck.cpp ] && cc -DXT_TONE_MAIN -DXT_TONE_CHECK XtToneCheck.cpp XtSynth.cpp XtSoloEncoder.cpp XtSoloVoice.cpp XtLinearPlayer.cpp XtLitePlayer.cpp XtMultiBandPlayer.cpp XtEnvelope.cpp XtLfo.cpp XtOsc.cpp \
   XtCosm.cpp XtAnalog.cpp "${FW[@]}" Sh4Interp.cpp "${VF[@]}" XtPatch.cpp "${SARP[@]}" \
   $( [ -f XtArpeggioPatch.cpp ] && echo XtArpeggioPatch.cpp ) $( [ -f XtSynthMsm.cpp ] && echo XtMsmPatch.cpp ) \
   $( [ -f XtToneMsm.cpp ] && echo XtToneFirmware.cpp XtToneCases.cpp XtToneCarry.cpp XtToneMsm.cpp ) \
   $( [ -f XtTonePatchIds.cpp ] && echo XtTonePatchIds.cpp ) \
   $( [ -f XtSynthRom.cpp ] && echo XtSynthRom.cpp ) -o "$O/xt_tone" &
[ -f XtFiles.cpp ] && cc -DXT_FILES_MAIN "${VF[@]}" "${FW[@]}" XtMultiBandPlayer.cpp \
   XtSoloVoice.cpp XtSoloEncoder.cpp Sh4Interp.cpp -o "$O/xt_files" &
[ -f XtFxChip.cpp ] && cc -DXT_FX_MAIN -I"$REPO/research/third_party/gearmulator_esp" XtFxChip.cpp XtFxControl.cpp Sh4Interp.cpp -o "$O/xt_fx" &
# The arpeggiator against the firmware's own object (v188), on the disk image's patches; the tempo ticks it steps on (v189)
ARP=(XtArpeggio.cpp XtArpeggioStep.cpp XtArpeggioMotifs.cpp XtArpeggioCheck.cpp XtArpeggioCases.cpp XtTempoTicks.cpp
     XtTempoTicksCheck.cpp XtArpeggioPatch.cpp)
[ -f XtArpeggio.cpp ] && cc -DXT_ARP_MAIN "${ARP[@]}" "${VF[@]}" "${FW[@]}" XtMultiBandPlayer.cpp \
   XtSoloVoice.cpp XtSoloEncoder.cpp Sh4Interp.cpp -o "$O/xt_arp" &
# the Multi Step Modulator (v192 on), with the patches' reads for the presets
[ -f XtMsm.cpp ] && cc -DXT_MSM_MAIN XtMsm.cpp XtMsmTrack.cpp XtMsmPatch.cpp XtMsmCheck.cpp XtMsmCases.cpp "${VF[@]}" \
   "${FW[@]}" XtMultiBandPlayer.cpp XtSoloVoice.cpp XtSoloEncoder.cpp Sh4Interp.cpp -o "$O/xt_msm" &
# Auto's classifier on a voiced and a pure tone (v191: what the legacy engine's tests kept of it)
CLS=$( [ -f ContentClassifier.cpp ] && [ -f ../Tests/ClassifierTest.cpp ] && echo classifier_test )
[ -n "$CLS" ] && cc ../Tests/ClassifierTest.cpp ContentClassifier.cpp -o "$O/classifier_test" &
cc -I. "$REPO/research/tools/rd.cpp" Sh4Interp.cpp -o "$O/rd" &
wait
for b in xt_osc xt_lfo xt_env xt_fw xt_enc xt_synth $CLS rd \
         $( [ -f XtCosm.cpp ] && echo xt_cosm ) $( [ -f XtAnalog.cpp ] && echo xt_analog ) \
         $( [ -f XtFxChip.cpp ] && echo xt_fx ) $( [ -f XtFiles.cpp ] && echo xt_files ) \
         $( [ -f XtToneCheck.cpp ] && echo xt_tone ) $( [ -f XtArpeggio.cpp ] && echo xt_arp ) \
         $( [ -f XtMsm.cpp ] && echo xt_msm ); do
  [ -x "$O/$b" ] || { echo "build: $b missing"; exit 1; }; done
