#!/bin/bash
# build.sh [src dir] [out dir]: every firmware harness and the XtSynth CLI from a
# plugin/Source tree (default the working tree) into out (default $WORK/bin).
source "$(dirname "$0")/env.sh"
S="${1:-$REPO/plugin/Source}"; O="${2:-$WORK/bin}"; mkdir -p "$O"
cc() { "$TC/clang++" -isysroot "$SDK" -std=c++20 -O2 "$@" 2>&1 | grep -E " error"; return 0; }
cd "$S" || exit 1
cc -DXT_OSC_MAIN XtOsc.cpp XtEnvelope.cpp XtFirmware.cpp XtMultiBandPlayer.cpp Sh4Interp.cpp -o "$O/xt_osc"
[ -f XtCosm.cpp ] && cc -DXT_COSM_MAIN XtCosm.cpp XtOsc.cpp XtEnvelope.cpp Sh4Interp.cpp -o "$O/xt_cosm"
cc -DXT_LFO_MAIN XtLfo.cpp Sh4Interp.cpp -o "$O/xt_lfo"
cc -DXT_ENV_MAIN XtEnvelope.cpp Sh4Interp.cpp -o "$O/xt_env"
cc -ffp-contract=off -DXT_FW_MAIN XtFirmware.cpp Sh4Interp.cpp XtMultiBandPlayer.cpp XtSoloVoice.cpp -o "$O/xt_fw"
[ -f XtAnalog.cpp ] && cc -DXT_ANALOG_MAIN XtAnalog.cpp XtOsc.cpp XtCosm.cpp XtEnvelope.cpp XtFirmware.cpp XtMultiBandPlayer.cpp Sh4Interp.cpp -o "$O/xt_analog"
cc -DXT_SYNTH_MAIN XtSynth.cpp XtSoloEncoder.cpp XtSoloVoice.cpp XtMultiBandPlayer.cpp XtEnvelope.cpp XtLfo.cpp XtOsc.cpp \
   $( [ -f XtCosm.cpp ] && echo XtCosm.cpp ) $( [ -f XtAnalog.cpp ] && echo XtAnalog.cpp ) XtFirmware.cpp Sh4Interp.cpp -o "$O/xt_synth"
cc -DENGINE_TESTS_MAIN EngineTests.cpp VariphraseEngine.cpp PhaseVocoder.cpp SourceFilterModel.cpp XtSoloEncoder.cpp XtSoloVoice.cpp \
   XtMultiBandPlayer.cpp Sh4Interp.cpp XtFirmware.cpp -o "$O/engine_tests"
cc -I. "$REPO/research/tools/rd.cpp" Sh4Interp.cpp -o "$O/rd"
for b in xt_osc xt_lfo xt_env xt_fw xt_synth engine_tests rd; do [ -x "$O/$b" ] || { echo "build: $b missing"; exit 1; }; done
