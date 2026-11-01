#!/bin/bash
# ab.sh [git rev]: XtSynth CLI renders of a revision (default HEAD) against the
# working tree: four inputs, two overlapping notes; XT_* settings pass through
# the environment.  One line: "n/4 identical", or each difference in dB.
source "$(dirname "$0")/env.sh"
REV=${1:-HEAD}; A="$WORK/ab"; mkdir -p "$A"
[ -f "$WORK/inputs/vocal.bin" ] || "$PY" "$REPO/research/tools/make_inputs.py" "$WORK/inputs" > /dev/null
rm -rf "$A/src" && mkdir -p "$A/src" && (cd "$REPO" && git archive "$REV" plugin/Source | tar -x -C "$A/src") || exit 1
cc() { "$TC/clang++" -isysroot "$SDK" -std=c++20 -O2 -DXT_SYNTH_MAIN "$@" 2>&1 | grep -E " error"; return 0; }
synth() { (cd "$1" && cc XtSynth.cpp XtSoloEncoder.cpp XtSoloVoice.cpp XtMultiBandPlayer.cpp XtEnvelope.cpp XtLfo.cpp XtOsc.cpp \
           $( [ -f XtCosm.cpp ] && echo XtCosm.cpp ) $( [ -f XtAnalog.cpp ] && echo XtAnalog.cpp ) XtFirmware.cpp Sh4Interp.cpp -o "$2"); }
rm -f "$A/old" "$A/new"; synth "$A/src/plugin/Source" "$A/old"; synth "$REPO/plugin/Source" "$A/new"
[ -x "$A/old" ] && [ -x "$A/new" ] || { echo "ab: build failed"; exit 1; }
ok=0; msg=""
for spec in "vocal 1 44100 1 48000" "drum 2 48000 3 48000" "w358 2 44100 1 44100" "vocal 1 44100 5 48000"; do
  set -- $spec
  for b in old new; do "$A/$b" "$ROM" "$WORK/inputs/$1.bin" $2 $3 $4 $5 60000 "$A/$1_$4_$b.f32" 60@100:30000 64@5000:45000 > /dev/null 2>&1; done
  r=$("$PY" -c "
import numpy as np
a=np.fromfile('$A/$1_$4_old.f32','<f4').astype(float); b=np.fromfile('$A/$1_$4_new.f32','<f4').astype(float)
d=a-b; e=np.sqrt(np.mean(a*a)); nz=np.nonzero(d)[0]
print('identical' if len(a) and len(nz)==0 else 'diff %.1f dB, first at frame %d' % (20*np.log10(np.sqrt(np.mean(d*d))/e), nz[0]//2) if len(nz) else 'no output')")
  [ "$r" = identical ] && ok=$((ok+1)) || msg="$msg\n  $1 type $4: $r"
done
echo -e "ab vs $REV: $ok/4 identical$msg"; [ $ok = 4 ]
