#!/bin/bash
# verify.sh [--full] [git rev]: the standard validation of a version, one line each —
# firmware checks against the baseline (--full: all of them run, none reused), A/B
# renders against a revision (default HEAD; settings from XT_* env), the plugin build,
# PluginTest (TEST_* env), the effects' panel, the editor's layout, the ROM search, a processor closed mid-encode, a patch through the parameters, a .vpw, the engine tests, the build id and auval.
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
# Waves on their stored vpdt: BACKING, ENSEMBLE and SOLO (373, whose stored records aren't the XT's encode; VP9SAMnoise1, its events between
# samples and its stream's own terminator, under EVENT; preset 270, its records short of its last event and a segment off their sum, under
# EVENT), and preset 270 with a terminator too short to reach its last event, encoded again on its own events, under EVENT
[ $BUILT = 1 ] && { n=0; "$WORK/fcheck/bin/xt_files" "$ROM" extract 270 "$WORK/verify_270.vpw" > /dev/null; "$PY" "$REPO/research/tools/vpdt_term.py" "$WORK/verify_270.vpw" "$WORK/verify_270t.vpw" 8
  for wm in "376 Synth014.wav:0:played" "362 Synth006.wav:3:played" "373 SynthFX002.wav:0:played" "002 VP9SAMnoise1.wav:3:played" "$WORK/verify_270.vpw:3:played" "$WORK/verify_270t.vpw:3:encoded"; do
  f=${wm%%:*}; m=${wm#*:}; m=${m%%:*}; [ "${f#/}" = "$f" ] && f="analysis/projects/Emu_test.prj/Wave/$f"
  o=$(cd "$REPO" && TEST_PLAYBACK=$m TEST_NOTE2="64,30000" build/plugin/VSynthEmuTest "$ROM" "$f" 60 auto 2>&1)
  echo "$o" | grep -q "plugin - XtSynth| = 0 (first at frame -1)" && echo "$o" | grep -q "(the wave's own)" && echo "$o" | grep -q "the wave's own vpdt: ${wm##*:}" && n=$((n+1)); done
  echo "stored waves, plugintest: $n/6 bit-identical"; [ $n = 6 ] || rc=1; }
# A test of this build, run in the checkout: its last line, and the run fails as it does.  VAR=value first goes to its
# environment.
checkTest() {
    local o
    o=$(cd "$REPO" && env "$@" 2>&1) || rc=1
    echo "$o" | tail -1
}
if [ $BUILT = 1 ]; then
    # PluginTest's own modes: the effects' panel, the ROM search (the editor's layout is VSynthEmuEditorTest's, v196)
    for mode in TEST_FX_PANEL TEST_ROM_SEARCH; do
        checkTest "$mode=1" build/plugin/VSynthEmuTest "$ROM" analysis/test_files/raw/chord_Cmaj_original.wav
    done
    # A processor closed while its wave is encoding (closed in ms, the next one encodes as before); a project that
    # reopens without its parts (each said); a wave whose header claims more than its data holds (refused at once);
    # another image given while one is in use (refused, the tables as they were); the harnesses' switches set (unread);
    # firmware routines that loop (each way gives up within its step limit);
    # damaged disk images (refused, or each cluster once); the arpeggiator in XtSynth, heard (a note a quarter, two tempos);
    # and its plugin side: its parameters against the firmware, a patch's, a session's, a chord through processBlock (v190);
    # the editor, D (v196, in TEST_UI_LAYOUT's place): its sizes, its layout on each page and block, every control on its
    # own parameter and every parameter with a control, each press's route
    for t in Teardown Restore WaveFile Rom Switch Step DiskImage Arp ArpPlugin Editor; do
        checkTest "build/plugin/VSynthEmu${t}Test" "$ROM"
    done
    # Every patch of the firmware's image through the parameters and effects, and a project on disk against XtSynth
    checkTest TEST_PATCH_APPLY=1 build/plugin/VSynthEmuTest "$ROM" x
fi
# A .vpw through the processor, against XtSynth on the same audio: a preset wave, mono and stereo, on its own encode type
[ $BUILT = 1 ] && { n=0; for w in 0 300; do "$WORK/fcheck/bin/xt_files" "$ROM" extract $w "$WORK/verify_$w.vpw" > /dev/null; o=$(cd "$REPO" && build/plugin/VSynthEmuTest "$ROM" "$WORK/verify_$w.vpw" 60 auto 2>&1); echo "$o" | grep -q "plugin - XtSynth| = 0 (first at frame -1)" && echo "$o" | grep -q "the wave's own" && n=$((n+1)); done; echo ".vpw, plugintest: $n/2 bit-identical"; [ $n = 2 ] || rc=1; }
# Auto's classifier on a voiced and a pure tone: the run fails as the program does, not only its text
o=$("$WORK/fcheck/bin/classifier_test" 2>&1) || rc=1
echo "classifier: $(echo "$o" | tail -1)"
# The tools' paths: env.sh sourced from bash and zsh, wherever from, on a copy in a tree of its own
bash "$D/env_test.sh" || rc=1
# A fresh clone can fill its gitlinks (JUCE): each has its URL in .gitmodules
bash "$D/gitlinks_test.sh" || rc=1
# Which build: the script on a repository of its own, then the copies this build made against git
bash "$D/buildid_test.sh" || rc=1
[ $BUILT = 1 ] && { bash "$D/buildid.sh" || rc=1; }
# The AU as a host sees it: the run fails unless auval succeeds and says so
if [ $BUILT = 1 ]; then
    o=$(auval -v aumu VsEm Gato 2>&1) || rc=1
    echo "auval: $(echo "$o" | grep -o "AU VALIDATION SUCCEEDED\|FAIL.*" | head -1)"
    echo "$o" | grep -q "AU VALIDATION SUCCEEDED" || rc=1
fi
exit $rc
