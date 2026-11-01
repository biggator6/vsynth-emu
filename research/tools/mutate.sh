#!/bin/bash
# mutate.sh <name> <old> <new> [cases] [pattern]: a deliberate error in a port, to
# show its firmware check catches it.  Copies plugin/Source/XtCosm.cpp (with
# PORT=analog, XtAnalog.cpp) with the first <old> replaced by <new> (exact
# text), builds xt_cosm (xt_analog) from it in $WORK/mut/<name>, runs it on
# [cases] (100) and prints the lines matching [pattern] (an extended regex;
# default every result line).  A check that
# still says 0 different missed the error: either the check is too weak, or
# the change is exact (e.g. a product of powers of two regrouped).
source "$(dirname "$0")/env.sh"
[ $# -ge 3 ] || { echo "usage: mutate.sh name old new [cases] [pattern]"; exit 2; }
N=$1; O=$2; W=$3; C=${4:-100}; P=${5:-.}
S="$REPO/plugin/Source"; M="$WORK/mut"; mkdir -p "$M"
if [ "${PORT:-cosm}" = analog ]; then
    F=XtAnalog.cpp; D=XT_ANALOG_MAIN
    L=("$S/XtOsc.cpp" "$S/XtCosm.cpp" "$S/XtEnvelope.cpp" "$S/XtFirmware.cpp" "$S/XtMultiBandPlayer.cpp" "$S/Sh4Interp.cpp")
else
    F=XtCosm.cpp; D=XT_COSM_MAIN; L=("$S/XtOsc.cpp" "$S/XtEnvelope.cpp" "$S/Sh4Interp.cpp")
fi
"$PY" - "$S/$F" "$M/$N.cpp" "$O" "$W" <<'PY' || exit 1
import sys
s = open(sys.argv[1]).read()
if s.count(sys.argv[3]) == 0: sys.exit('mutate: the old text is not in ' + sys.argv[1])
open(sys.argv[2], 'w').write(s.replace(sys.argv[3], sys.argv[4], 1))
PY
"$TC/clang++" -isysroot "$SDK" -std=c++20 -O2 -D$D -I"$S" "$M/$N.cpp" "${L[@]}" -o "$M/$N" 2>&1 | grep -E " error" && exit 1
echo "== $N"
"$M/$N" "$ROM" "$C" | grep -v "^  " | grep -E "$P"
