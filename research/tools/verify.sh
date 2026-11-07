#!/bin/bash
# verify.sh [--full] [git rev]: the standard validation of a version, one line each —
# firmware checks against the baseline (--full: all of them run, none reused), A/B
# renders against a revision (default HEAD; settings from XT_* env), the plugin build,
# PluginTest (TEST_* env), the effects' panel, the editor's layout, the ROM search, a patch through the parameters, a .vpw, the engine tests, the build id and auval.
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
# And with Vari Sw off: SOLO on XtLinearPlayer, the band players' walk on the pitch
[ $BUILT = 1 ] && { echo -n "vari off, "; TEST_VARI=0 bash "$D/pt.sh" || rc=1; }
# And with a Start Offset: where each player starts, and where a walk back stops
[ $BUILT = 1 ] && { echo -n "start offset, "; TEST_START=3000 bash "$D/pt.sh" || rc=1; }
[ $BUILT = 1 ] && { echo -n "playback mode, "; TEST_PLAYBACK=2 TEST_OSC2_PLAYBACK=1 TEST_NOTE2="64,30000" bash "$D/pt.sh" || rc=1; }
# The project's LITE waves, mono and stereo, as their own type, with Vari on (XtLitePlayer) and off (XtLinearPlayer)
[ $BUILT = 1 ] && { n=0; for w in "005 sine_400hz_r.wav" "348 Handclap.wav"; do for v in 1 0; do o=$(cd "$REPO" && TEST_VARI=$v build/plugin/VSynthEmuTest "$ROM" "analysis/projects/Emu_test.prj/Wave/$w" 60 auto 2>&1)
  echo "$o" | grep -q "plugin - XtSynth| = 0 (first at frame -1)" && echo "$o" | grep -q "Encoded as LITE" && n=$((n+1)); done; done; echo "LITE waves, plugintest: $n/4 bit-identical"; [ $n = 4 ] || rc=1; }
# A project's SOLO wave with attacks on its own events (Tribal1, 7), with STEP and EVENT
[ $BUILT = 1 ] && { n=0; for m in 2 3; do o=$(cd "$REPO" && TEST_PLAYBACK=$m TEST_NOTE2="64,30000" build/plugin/VSynthEmuTest "$ROM" "analysis/projects/Emu_test.prj/Wave/346 Tribal1.wav" 60 auto 2>&1)
  echo "$o" | grep -q "plugin - XtSynth| = 0 (first at frame -1)" && echo "$o" | grep -q "Encoded as SOLO (the wave's own)" && n=$((n+1)); done; echo "SOLO events, plugintest: $n/2 bit-identical"; [ $n = 2 ] || rc=1; }
# Project waves on their stored vpdt: BACKING, ENSEMBLE and SOLO (373, whose stored records aren't the XT's encode), and VP9SAMnoise1, encoded again
# on its own events as they fall between samples, under EVENT
[ $BUILT = 1 ] && { n=0; for wm in "376 Synth014.wav:0" "362 Synth006.wav:3" "373 SynthFX002.wav:0" "002 VP9SAMnoise1.wav:3"; do o=$(cd "$REPO" && TEST_PLAYBACK=${wm##*:} TEST_NOTE2="64,30000" build/plugin/VSynthEmuTest "$ROM" "analysis/projects/Emu_test.prj/Wave/${wm%:*}" 60 auto 2>&1)
  echo "$o" | grep -q "plugin - XtSynth| = 0 (first at frame -1)" && echo "$o" | grep -q "(the wave's own)" && n=$((n+1)); done; echo "stored waves, plugintest: $n/4 bit-identical"; [ $n = 4 ] || rc=1; }
[ $BUILT = 1 ] && { o=$(cd "$REPO" && TEST_FX_PANEL=1 build/plugin/VSynthEmuTest "$ROM" analysis/test_files/raw/chord_Cmaj_original.wav 2>&1) || rc=1; echo "$o" | tail -1; }
[ $BUILT = 1 ] && { o=$(cd "$REPO" && TEST_UI_LAYOUT=1 build/plugin/VSynthEmuTest "$ROM" analysis/test_files/raw/chord_Cmaj_original.wav 2>&1) || rc=1; echo "$o" | tail -1; }
[ $BUILT = 1 ] && { o=$(cd "$REPO" && TEST_ROM_SEARCH=1 build/plugin/VSynthEmuTest "$ROM" analysis/test_files/raw/chord_Cmaj_original.wav 2>&1) || rc=1; echo "$o" | tail -1; }
# Every patch of the firmware's image through the parameters and effects, and a project on disk against XtSynth
[ $BUILT = 1 ] && { o=$(cd "$REPO" && TEST_PATCH_APPLY=1 build/plugin/VSynthEmuTest "$ROM" x 2>&1) || rc=1; echo "$o" | tail -1; }
# A .vpw through the processor, against XtSynth on the same audio: a preset wave, mono and stereo, on its own encode type
[ $BUILT = 1 ] && { n=0; for w in 0 300; do "$WORK/fcheck/bin/xt_files" "$ROM" extract $w "$WORK/verify_$w.vpw" > /dev/null; o=$(cd "$REPO" && build/plugin/VSynthEmuTest "$ROM" "$WORK/verify_$w.vpw" 60 auto 2>&1); echo "$o" | grep -q "plugin - XtSynth| = 0 (first at frame -1)" && echo "$o" | grep -q "the wave's own" && n=$((n+1)); done; echo ".vpw, plugintest: $n/2 bit-identical"; [ $n = 2 ] || rc=1; }
echo "engine tests: $("$WORK/fcheck/bin/engine_tests" 2>&1 | tail -1)"
# Which build: the script on a repository of its own, then the copies this build made against git
bash "$D/buildid_test.sh" || rc=1
[ $BUILT = 1 ] && { bash "$D/buildid.sh" || rc=1; }
[ $BUILT = 1 ] && echo "auval: $(auval -v aumu VsEm Ystu 2>&1 | grep -o "AU VALIDATION SUCCEEDED\|FAIL.*" | head -1)"
exit $rc
