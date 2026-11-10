#!/bin/bash
# build.sh [src dir] [out dir]: every firmware harness and the XtSynth CLI from a
# plugin/Source tree (default the working tree) into out (default $WORK/bin), side by
# side.  What does not build is left missing, not as the last build's binary.
source "$(dirname "$0")/env.sh"
S="${1:-$REPO/plugin/Source}"; O="${2:-$WORK/bin}"; mkdir -p "$O"
cc() { "$TC/clang++" -isysroot "$SDK" -std=c++20 -O2 "${HARNESS_SWITCHES[@]}" "$@" 2>&1 | grep -E " error"; return 0; }
cd "$S" || exit 1
FW=(XtFirmware.cpp $( [ -f XtFwMachine.cpp ] && echo XtFwMachine.cpp ))   # the routines and their machine (v175 on)
# the legacy engine's source-filter model, and its formant analysis and root finder (v176 on)
SFM=(SourceFilterModel.cpp $( [ -f DownsampledFormants.cpp ] && echo DownsampledFormants.cpp PolynomialRoots.cpp ))
rm -f "$O"/{xt_osc,xt_cosm,xt_lfo,xt_env,xt_fw,xt_enc,xt_analog,xt_synth,xt_tone,xt_fx,xt_files,engine_tests,rd}
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
   Sh4Interp.cpp $( [ -f XtSynthRom.cpp ] && echo XtSynthRom.cpp ) -o "$O/xt_synth" &
[ -f XtToneCheck.cpp ] && cc -DXT_TONE_MAIN -DXT_TONE_CHECK XtToneCheck.cpp XtSynth.cpp XtSoloEncoder.cpp XtSoloVoice.cpp XtLinearPlayer.cpp XtLitePlayer.cpp XtMultiBandPlayer.cpp XtEnvelope.cpp XtLfo.cpp XtOsc.cpp \
   XtCosm.cpp XtAnalog.cpp "${FW[@]}" Sh4Interp.cpp XtFiles.cpp XtWaveFile.cpp XtPatch.cpp \
   $( [ -f XtSynthRom.cpp ] && echo XtSynthRom.cpp ) -o "$O/xt_tone" &
[ -f XtFiles.cpp ] && cc -DXT_FILES_MAIN XtFiles.cpp XtWaveFile.cpp "${FW[@]}" XtMultiBandPlayer.cpp \
   XtSoloVoice.cpp XtSoloEncoder.cpp Sh4Interp.cpp -o "$O/xt_files" &
[ -f XtFxChip.cpp ] && cc -DXT_FX_MAIN -I"$REPO/research/third_party/gearmulator_esp" XtFxChip.cpp XtFxControl.cpp Sh4Interp.cpp -o "$O/xt_fx" &
cc -DENGINE_TESTS_MAIN EngineTests.cpp VariphraseEngine.cpp PhaseVocoder.cpp "${SFM[@]}" \
   XtSoloEncoder.cpp XtSoloVoice.cpp XtMultiBandPlayer.cpp Sh4Interp.cpp "${FW[@]}" -o "$O/engine_tests" &
cc -I. "$REPO/research/tools/rd.cpp" Sh4Interp.cpp -o "$O/rd" &
wait
for b in xt_osc xt_lfo xt_env xt_fw xt_enc xt_synth engine_tests rd $( [ -f XtCosm.cpp ] && echo xt_cosm ) $( [ -f XtAnalog.cpp ] && echo xt_analog ) $( [ -f XtFxChip.cpp ] && echo xt_fx ) $( [ -f XtFiles.cpp ] && echo xt_files ) $( [ -f XtToneCheck.cpp ] && echo xt_tone ); do
  [ -x "$O/$b" ] || { echo "build: $b missing"; exit 1; }; done
