#!/bin/bash
# mutate.sh <name> <old> <new> [cases] [pattern]: a deliberate error in a port, to
# show its firmware check catches it.  Copies plugin/Source/XtCosm.cpp (with
# PORT=analog, XtAnalog.cpp; with PORT=analogcosm, XtCosm.cpp under xt_analog's
# check; with PORT=tone, XtSynth.cpp under xt_tone) with the first <old> replaced by
# <new> (exact text), builds xt_cosm (xt_analog, xt_tone) from it in $WORK/mut/<name>, runs it on
# [cases] (100) and prints the lines matching [pattern] (an extended regex;
# default every result line).  A check that
# still says 0 different missed the error: either the check is too weak, or
# the change is exact (e.g. a product of powers of two regrouped).
source "$(dirname "$0")/env.sh"
[ $# -ge 3 ] || { echo "usage: mutate.sh name old new [cases] [pattern]"; exit 2; }
N=$1; O=$2; W=$3; C=${4:-100}; P=${5:-.}
S="$REPO/plugin/Source"; M="$WORK/mut"; mkdir -p "$M"
if [ "${PORT:-cosm}" = tone ]; then                    # XtSynth's wiring, against xt_tone (the rest built once)
    F=XtSynth.cpp; D=XT_TONE_MAIN; ARGS=(2024 "$WORK/inputs")
    OBJ="$M/tone_obj"; mkdir -p "$OBJ"; L=()
    for f in XtToneCheck XtSoloEncoder XtSoloVoice XtMultiBandPlayer XtEnvelope XtLfo XtOsc XtCosm XtAnalog XtFirmware Sh4Interp; do
        [ "$OBJ/$f.o" -nt "$S/$f.cpp" ] || "$TC/clang++" -isysroot "$SDK" -std=c++20 -O2 -DXT_TONE_MAIN -DXT_TONE_CHECK -c "$S/$f.cpp" -o "$OBJ/$f.o" 2>&1 | grep -E " error"
        L+=("$OBJ/$f.o")
    done
    EXTRA=(-DXT_TONE_CHECK)
elif [ "${PORT:-cosm}" = analog ]; then
    F=XtAnalog.cpp; D=XT_ANALOG_MAIN
    L=("$S/XtOsc.cpp" "$S/XtCosm.cpp" "$S/XtEnvelope.cpp" "$S/XtFirmware.cpp" "$S/XtMultiBandPlayer.cpp" "$S/Sh4Interp.cpp")
elif [ "$PORT" = analogcosm ]; then
    F=XtCosm.cpp; D=XT_ANALOG_MAIN
    L=("$S/XtOsc.cpp" "$S/XtAnalog.cpp" "$S/XtEnvelope.cpp" "$S/XtFirmware.cpp" "$S/XtMultiBandPlayer.cpp" "$S/Sh4Interp.cpp")
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
"$M/$N" "$ROM" "$C" "${ARGS[@]}" | grep -v "^  " | grep -E "$P"
