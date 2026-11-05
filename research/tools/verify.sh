#!/bin/bash
# verify.sh [--full] [git rev]: the standard validation of a version, one line each —
# firmware checks against the baseline (--full: all of them run, none reused), A/B
# renders against a revision (default HEAD; settings from XT_* env), the plugin build,
# PluginTest (TEST_* env), the effects' panel, the editor's layout, the engine tests and auval.
source "$(dirname "$0")/env.sh"
D="$(dirname "$0")"; rc=0; F=""; BUILT=1
[ "$1" = "--full" ] && { F=--full; shift; }
bash "$D/fcheck.sh" $F || rc=1
bash "$D/ab.sh" "${1:-HEAD}" || rc=1
# A Makefile from before the target has no rule for it, and make does not regenerate for a target it does not know
(cd "$REPO" && { grep -q VSynthEmuVerify build/plugin/Makefile 2>/dev/null || SDKROOT="$SDK" cmake -S plugin -B build/plugin -DCMAKE_BUILD_TYPE=Release; } > "$WORK/build_plugin.log" 2>&1 \
  && SDKROOT="$SDK" cmake --build build/plugin --target VSynthEmuVerify -j 10 >> "$WORK/build_plugin.log" 2>&1) \
  && echo "plugin: built" || { echo "plugin: BUILD FAILED (see $WORK/build_plugin.log)"; rc=1; BUILT=0; }
[ $BUILT = 1 ] && { bash "$D/pt.sh" || rc=1; }     # without a build, the old binaries would pass
# And through the chip, with the effects set and then edited in play, against XtSynth and XtFxUnit by hand
[ $BUILT = 1 ] && { echo -n "effects, "; TEST_FX=1 TEST_FX_KNOBS="fxMfxOn=1,fxMfxType=23,fxMfxCho=60,fxMfxRev=60,fxChoOn=1,fxChoType=1,fxChoRev=30,fxRevOn=1,fxRevType=4,fxMaster=110,fxGain=2" TEST_FX_EDIT="0x66=20" \
  TEST_BUS="60,80,0,1" TEST_MATRIX="1,75,63,0,0" TEST_CC="1,127,30000" TEST_FX_LIVE="24000:fxRevType=8,36000:0x67=20,48000:fxChoType=6,50000:fxMfxOn=0,50400:fxMfxOn=1,60000:fxMfxType=11,72000:fxGain=-6" bash "$D/pt.sh" || rc=1; }
[ $BUILT = 1 ] && { o=$(cd "$REPO" && TEST_FX_PANEL=1 build/plugin/VSynthEmuTest "$ROM" analysis/test_files/raw/chord_Cmaj_original.wav 2>&1) || rc=1; echo "$o" | tail -1; }
[ $BUILT = 1 ] && { o=$(cd "$REPO" && TEST_UI_LAYOUT=1 build/plugin/VSynthEmuTest "$ROM" analysis/test_files/raw/chord_Cmaj_original.wav 2>&1) || rc=1; echo "$o" | tail -1; }
echo "engine tests: $("$WORK/fcheck/bin/engine_tests" 2>&1 | tail -1)"
[ $BUILT = 1 ] && echo "auval: $(auval -v aumu VsEm Ystu 2>&1 | grep -o "AU VALIDATION SUCCEEDED\|FAIL.*" | head -1)"
exit $rc
