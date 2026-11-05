#!/bin/bash
# mutate.sh <name> <old> <new> [cases] [pattern]: a deliberate error in a port, to
# show its firmware check catches it.  Copies plugin/Source/XtCosm.cpp (with
# PORT=analog, XtAnalog.cpp; with PORT=analogcosm, XtCosm.cpp, PORT=analogband, XtMultiBandPlayer.cpp, and PORT=analogfw, XtFirmware.cpp, under xt_analog's
# check (XT_ANALOG_ONLY=pcm, the PCM pass alone, is passed through); with PORT=tone, XtSynth.cpp, PORT=tonecosm, XtCosm.cpp, PORT=toneanalog, XtAnalog.cpp, PORT=toneband, XtMultiBandPlayer.cpp (XtBeat too), PORT=tonecheck, XtToneCheck.cpp, PORT=tonefw, XtFirmware.cpp, PORT=toneosc, XtOsc.cpp, PORT=tonelfo, XtLfo.cpp, PORT=toneenv, XtEnvelope.cpp, and PORT=tonesolo, XtSoloVoice.cpp, PORT=tonepatch, XtPatch.cpp, and PORT=tonefiles, XtFiles.cpp, under xt_tone,
# on seed $SEED, default 2024; with PORT=solo, XtSoloVoice.cpp under xt_fw solocmp; with PORT=enc, XtSoloEncoder.cpp under
# xt_enc on fcheck's waves, which prints only those that differ; with PORT=fx, XtFxChip.cpp under xt_fx; with PORT=fxc, XtFxControl.cpp under xt_fx control, [cases] its events) with the first <old> replaced by
# <new> (exact text), builds xt_cosm (xt_analog, xt_tone, xt_fw, xt_enc) from it in $WORK/mut/<name>, runs it on
# [cases] (100) and prints the lines matching [pattern] (an extended regex;
# default every result line).  A check that
# still says 0 different missed the error: either the check is too weak, or
# the change is exact (e.g. a product of powers of two regrouped).
source "$(dirname "$0")/env.sh"
[ $# -ge 3 ] || { echo "usage: mutate.sh name old new [cases] [pattern]"; exit 2; }
N=$1; O=$2; W=$3; C=${4:-100}; P=${5:-.}
S="$REPO/plugin/Source"; M="$WORK/mut"; mkdir -p "$M"
if [ "${PORT:-cosm}" = tone ] || [ "$PORT" = tonecosm ] || [ "$PORT" = toneanalog ] || [ "$PORT" = toneband ] || [ "$PORT" = tonecheck ] || [ "$PORT" = tonefw ] || [ "$PORT" = toneosc ] || [ "$PORT" = tonelfo ] || [ "$PORT" = toneenv ] || [ "$PORT" = tonesolo ] || [ "$PORT" = tonelin ] || [ "$PORT" = tonepatch ] || [ "$PORT" = tonefiles ]; then   # XtSynth's wiring, against xt_tone (the rest built once)
    F=XtSynth.cpp; [ "$PORT" = tonelin ] && F=XtLinearPlayer.cpp; [ "$PORT" = tonecosm ] && F=XtCosm.cpp; [ "$PORT" = toneanalog ] && F=XtAnalog.cpp; [ "$PORT" = toneband ] && F=XtMultiBandPlayer.cpp; [ "$PORT" = tonecheck ] && F=XtToneCheck.cpp; [ "$PORT" = tonefw ] && F=XtFirmware.cpp; [ "$PORT" = toneosc ] && F=XtOsc.cpp; [ "$PORT" = tonelfo ] && F=XtLfo.cpp; [ "$PORT" = toneenv ] && F=XtEnvelope.cpp; [ "$PORT" = tonesolo ] && F=XtSoloVoice.cpp; [ "$PORT" = tonepatch ] && F=XtPatch.cpp; [ "$PORT" = tonefiles ] && F=XtFiles.cpp
    D=XT_TONE_MAIN; ARGS=("${SEED:-2024}" "$WORK/inputs")
    OBJ="$M/tone_obj"; mkdir -p "$OBJ"; L=()
    H=$(ls -t "$S"/*.h | head -1)   # a changed header rebuilds them all
    for f in XtToneCheck XtSynth XtSoloEncoder XtSoloVoice XtLinearPlayer XtMultiBandPlayer XtEnvelope XtLfo XtOsc XtCosm XtAnalog XtFirmware Sh4Interp XtFiles XtPatch; do
        [ "$f.cpp" = "$F" ] && continue
        [ "$OBJ/$f.o" -nt "$S/$f.cpp" ] && [ "$OBJ/$f.o" -nt "$H" ] || "$TC/clang++" -isysroot "$SDK" -std=c++20 -O2 -DXT_TONE_MAIN -DXT_TONE_CHECK -c "$S/$f.cpp" -o "$OBJ/$f.o" 2>&1 | grep -E " error"
        L+=("$OBJ/$f.o")
    done
    EXTRA=(-DXT_TONE_CHECK)
elif [ "$PORT" = solo ]; then      # XtSoloVoice, on the solocmp cases of fcheck's baseline; [cases] is not used
    F=XtSoloVoice.cpp; D=XT_FW_MAIN; L=("$S/XtFirmware.cpp" "$S/XtMultiBandPlayer.cpp" "$S/Sh4Interp.cpp"); EXTRA=(-ffp-contract=off)
elif [ "$PORT" = fx ]; then        # XtFxChip against the reference; [cases] is its samples per type, and XT_FX_STOP stops at the first difference
    F=XtFxChip.cpp; D=XT_FX_MAIN; EXTRA=(-I"$REPO/research/third_party/gearmulator_esp")
    OBJ="$M/fx_obj"; mkdir -p "$OBJ"
    for f in Sh4Interp XtFxControl; do [ "$OBJ/$f.o" -nt "$S/$f.cpp" ] && [ "$OBJ/$f.o" -nt "$S/XtFxControl.h" ] && [ "$OBJ/$f.o" -nt "$S/XtFxChip.h" ] || { "$TC/clang++" -isysroot "$SDK" -std=c++20 -O2 -c "$S/$f.cpp" -o "$OBJ/$f.$$.o" && mv "$OBJ/$f.$$.o" "$OBJ/$f.o"; }; done
    L=("$OBJ/Sh4Interp.o" "$OBJ/XtFxControl.o")
elif [ "$PORT" = fxc ]; then       # XtFxControl against its checks in xt_fx; [cases] is the events of "xt_fx control"
    F=XtFxControl.cpp; D=XT_FX_MAIN; EXTRA=(-I"$REPO/research/third_party/gearmulator_esp")
    OBJ="$M/fx_obj"; mkdir -p "$OBJ"
    [ "$OBJ/Sh4Interp.o" -nt "$S/Sh4Interp.cpp" ] || { "$TC/clang++" -isysroot "$SDK" -std=c++20 -O2 -c "$S/Sh4Interp.cpp" -o "$OBJ/Sh4Interp.$$.o" && mv "$OBJ/Sh4Interp.$$.o" "$OBJ/Sh4Interp.o"; }
    [ "$OBJ/XtFxChip_main.o" -nt "$S/XtFxChip.cpp" ] && [ "$OBJ/XtFxChip_main.o" -nt "$S/XtFxControl.h" ] && [ "$OBJ/XtFxChip_main.o" -nt "$S/XtFxChip.h" ] || { "$TC/clang++" -isysroot "$SDK" -std=c++20 -O2 -DXT_FX_MAIN "${EXTRA[@]}" -I"$S" -c "$S/XtFxChip.cpp" -o "$OBJ/XtFxChip_main.$$.o" && mv "$OBJ/XtFxChip_main.$$.o" "$OBJ/XtFxChip_main.o"; }
    L=("$OBJ/Sh4Interp.o" "$OBJ/XtFxChip_main.o")
elif [ "$PORT" = enc ]; then       # XtSoloEncoder, on the xt_enc waves of fcheck's baseline; [cases] is not used
    F=XtSoloEncoder.cpp; D=XT_ENCODER_MAIN; L=("$S/XtFirmware.cpp" "$S/XtMultiBandPlayer.cpp" "$S/XtSoloVoice.cpp" "$S/Sh4Interp.cpp"); EXTRA=(-ffp-contract=off)
elif [ "$PORT" = files ]; then     # XtFiles against fcheck's baseline of xt_files; [cases] is not used
    F=XtFiles.cpp; D=XT_FILES_MAIN; L=("$S/XtFirmware.cpp" "$S/XtMultiBandPlayer.cpp" "$S/XtSoloVoice.cpp" "$S/XtSoloEncoder.cpp" "$S/Sh4Interp.cpp")
elif [ "$PORT" = filesfw ]; then   # the same, with the error in XtFirmware's routines for them
    F=XtFirmware.cpp; D=XT_FILES_MAIN
    OBJ="$M/files_obj"; mkdir -p "$OBJ"
    [ "$OBJ/XtFiles_main.o" -nt "$S/XtFiles.cpp" ] && [ "$OBJ/XtFiles_main.o" -nt "$S/XtFiles.h" ] && [ "$OBJ/XtFiles_main.o" -nt "$S/XtFirmware.h" ] || { "$TC/clang++" -isysroot "$SDK" -std=c++20 -O2 -DXT_FILES_MAIN -I"$S" -c "$S/XtFiles.cpp" -o "$OBJ/XtFiles_main.$$.o" && mv "$OBJ/XtFiles_main.$$.o" "$OBJ/XtFiles_main.o"; }
    L=("$OBJ/XtFiles_main.o" "$S/XtMultiBandPlayer.cpp" "$S/XtSoloVoice.cpp" "$S/XtSoloEncoder.cpp" "$S/Sh4Interp.cpp")
elif [ "${PORT:-cosm}" = analog ]; then
    F=XtAnalog.cpp; D=XT_ANALOG_MAIN
    L=("$S/XtOsc.cpp" "$S/XtCosm.cpp" "$S/XtEnvelope.cpp" "$S/XtFirmware.cpp" "$S/XtMultiBandPlayer.cpp" "$S/XtSoloVoice.cpp" "$S/Sh4Interp.cpp")
elif [ "$PORT" = analogcosm ]; then
    F=XtCosm.cpp; D=XT_ANALOG_MAIN
    L=("$S/XtOsc.cpp" "$S/XtAnalog.cpp" "$S/XtEnvelope.cpp" "$S/XtFirmware.cpp" "$S/XtMultiBandPlayer.cpp" "$S/XtSoloVoice.cpp" "$S/Sh4Interp.cpp")
elif [ "$PORT" = analogband ]; then
    F=XtMultiBandPlayer.cpp; D=XT_ANALOG_MAIN
    L=("$S/XtOsc.cpp" "$S/XtCosm.cpp" "$S/XtAnalog.cpp" "$S/XtEnvelope.cpp" "$S/XtFirmware.cpp" "$S/XtSoloVoice.cpp" "$S/Sh4Interp.cpp")
elif [ "$PORT" = analogfw ]; then
    F=XtFirmware.cpp; D=XT_ANALOG_MAIN
    L=("$S/XtOsc.cpp" "$S/XtCosm.cpp" "$S/XtAnalog.cpp" "$S/XtEnvelope.cpp" "$S/XtMultiBandPlayer.cpp" "$S/XtSoloVoice.cpp" "$S/Sh4Interp.cpp")
else
    F=XtCosm.cpp; D=XT_COSM_MAIN; L=("$S/XtOsc.cpp" "$S/XtEnvelope.cpp" "$S/Sh4Interp.cpp")
fi
"$PY" - "$S/$F" "$M/$N.cpp" "$O" "$W" <<'PY' || exit 1
import sys
s = open(sys.argv[1]).read()
if s.count(sys.argv[3]) == 0: sys.exit('mutate: the old text is not in ' + sys.argv[1])
open(sys.argv[2], 'w').write(s.replace(sys.argv[3], sys.argv[4], 1))
PY
"$TC/clang++" -isysroot "$SDK" -std=c++20 -O2 -D$D "${EXTRA[@]}" -I"$S" "$M/$N.cpp" "${L[@]}" -o "$M/$N" 2>&1 | grep -E " error" && exit 1
echo "== $N"
if [ "$PORT" = solo ]; then
    B="$REPO/research/tools/fcheck_baseline.txt"; T="$M/$N.out"; rm -rf "$T"; mkdir -p "$T"; i=0
    cd "$WORK/inputs"
    while IFS= read -r spec; do
        i=$((i+1))
        ( l="solo $spec: $("$M/$N" "$ROM" solocmp $spec 2>/dev/null | tail -1)"
          if grep -qxF -- "$l" "$B"; then echo "same: $spec"; else echo "DIFFERENT: $l"; fi ) > "$T/$i" &
    done < <(sed -n 's/^solo \(.*\): [0-9]* records.*/\1/p' "$B")
    wait
    cat "$T"/* | grep -E "$P"
    exit
fi
if [ "$PORT" = files ] || [ "$PORT" = filesfw ]; then
    B="$REPO/research/tools/fcheck_baseline.txt"; T="$M/$N.base"; grep -E '^(rdac|patches|short): ' "$B" > "$T"
    "$M/$N" "$ROM" 2>/dev/null | grep -vxF -f "$T" > "$M/$N.out"
    st=${PIPESTATUS[0]}; [ "$st" -ge 128 ] && echo "x: the run died, signal $((st - 128))" >> "$M/$N.out"
    grep -E "$P" "$M/$N.out"
    echo "files: $(grep -c . "$M/$N.out") lines differ from the baseline"
    exit
fi
if [ "$PORT" = enc ]; then
    B="$REPO/research/tools/fcheck_baseline.txt"; T="$M/$N.base"; grep '^enc ' "$B" > "$T"
    cd "$WORK/inputs"
    sed -n 's/^enc \([^:]*\(:2\)\{0,1\}\): .*/\1/p' "$T" | xargs -n 1 -P "${JOBS:-8}" "$M/$N" "$ROM" cmp 2>/dev/null | grep -vxF -f "$T" > "$M/$N.out"
    grep -E "$P" "$M/$N.out"
    echo "enc: $(grep -c . "$M/$N.out") waves differ from the baseline"
    exit
fi
[ "$PORT" = fxc ] && ARGS=("$C") && C=control
"$M/$N" "$ROM" "$C" "${ARGS[@]}" | grep -v "^  " | grep -E "$P"
st=${PIPESTATUS[0]}; [ "$st" -ge 128 ] && echo "$N: the run died, signal $((st - 128))"
