#!/bin/bash
# fcheck.sh [--update] [src dir]: every firmware check — the ported code against
# rom.2's own routines run in the SH-4 interpreter — on fixed cases.  Prints one
# line: "as baseline" (research/tools/fcheck_baseline.txt), or the lines that
# differ.  Full output: $WORK/fcheck/results.txt.  --update rewrites the
# baseline: only when a change in the results is intended (new checks, new cases).
source "$(dirname "$0")/env.sh"
UPDATE=0; [ "$1" = "--update" ] && { UPDATE=1; shift; }
S="${1:-$REPO/plugin/Source}"; O="$WORK/fcheck"; B="$REPO/research/tools/fcheck_baseline.txt"
mkdir -p "$O"; "$REPO/research/tools/build.sh" "$S" "$O/bin" || exit 1
[ -f "$WORK/inputs/vocal.bin" ] || "$PY" "$REPO/research/tools/make_inputs.py" "$WORK/inputs" > /dev/null
X="$O/bin"
{
  echo "== xt_osc"; "$X/xt_osc" "$ROM" | grep -v "^  "
  [ -x "$X/xt_cosm" ] && { echo "== xt_cosm"; "$X/xt_cosm" "$ROM" 3000 | grep -v "^  "; }
  echo "== xt_lfo"; "$X/xt_lfo" "$ROM" | tail -18
  echo "== xt_env"; "$X/xt_env" "$ROM" conv | tail -3
  for c in "0 127 0 100 40 300 2" "10 60 64 30 50 400 2" "50 90 20 80 120 600 0" "127 127 127 127 200 900 2" "0 0 127 0 5 60 0" "30 70 90 50 80 500 2"; do
    echo "env $c: $("$X/xt_env" "$ROM" $c | tail -1)"; done
  cd "$WORK/inputs"
  # solocmp <file> <channels> <R pitch ratio> <F formant ratio, 0 = none> <walk> <energy gain 0..1> <frames> [frame:walk ...]
  echo "== solocmp"
  for spec in "vocal.bin 1 1.0 0 1.0 0 30000" "vocal.bin 1 1.5 1.25 1.0 0.25 30000" "vocal.bin 1 0.7 0.8 2.0 0.6 30000" "vocal.bin 1 1.0 0 -1.0 0 30000" \
              "vocal.bin 1 1.2 1.1 0.5 0.05 30000 4000:-1.0 9000:2.0" "w358.bin 2 1.0 0 1.0 0 20000" "w358.bin 2 0.8 1.5 1.5 0.4 20000"; do
    echo "solo $spec: $("$X/xt_fw" "$ROM" solocmp $spec 2>/dev/null | tail -1)"; done
  # bandcmp <3 BACKING|5 ENSEMBLE> <file> <channels> <pitch> <walk> <frames> [frame:walk ...]
  echo "== bandcmp"
  for spec in "3 drum.bin 2 1.0 1.0 30000" "3 drum.bin 2 1.3 -1.0 30000" "5 chord.bin 1 0.8 1.5 30000" "5 chord.bin 1 1.0 0.0 20000 3000:1.0 8000:-0.5"; do
    echo "band $spec: $("$X/xt_fw" "$ROM" bandcmp $spec 2>&1 | tail -1)"; done
} > "$O/results.txt" 2>&1
if [ $UPDATE = 1 ]; then cp "$O/results.txt" "$B"; echo "fcheck: baseline updated ($(wc -l < "$B") lines)"; exit 0; fi
if diff -q "$B" "$O/results.txt" > /dev/null; then
  echo "fcheck: as baseline ($(grep -c "different\|identical\|differ" "$O/results.txt") checks)"
else
  echo "fcheck: DIFFERS from baseline:"; diff "$B" "$O/results.txt" | grep "^[<>]" | head -20; exit 1
fi
