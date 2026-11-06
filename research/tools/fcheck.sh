#!/bin/bash
# fcheck.sh [--update] [--full] [src dir]: every firmware check — the ported code
# against rom.2's own routines run in the SH-4 interpreter — on fixed cases, the
# sections side by side.  Prints one line: "as baseline"
# (research/tools/fcheck_baseline.txt), or the lines that differ.  Full output:
# $WORK/fcheck/results.txt.  A section is not run again when its binary, command, ROM
# and inputs are those of a run that matched the baseline: the builds are repeatable
# byte for byte, so the output would be the same.  --full runs them all (before a
# push).  --update rewrites the baseline from a full run: only when a change in the
# results is intended (new checks, new cases).
source "$(dirname "$0")/env.sh"
UPDATE=0; FULL=0
while :; do case "$1" in --update) UPDATE=1; FULL=1; shift ;; --full) FULL=1; shift ;; *) break ;; esac; done
S="${1:-$REPO/plugin/Source}"; O="$WORK/fcheck"; B="$REPO/research/tools/fcheck_baseline.txt"
mkdir -p "$O"; bash "$REPO/research/tools/build.sh" "$S" "$O/bin" || exit 1
[ -f "$WORK/inputs/enc2_zeros.bin" ] || "$PY" "$REPO/research/tools/make_inputs.py" "$WORK/inputs" > /dev/null
X="$O/bin"; P="$O/parts"; rm -rf "$P"; mkdir -p "$P"
K="$O/kept"                                                 # the kept sections, one store for all the worktrees
G=$(cd "$REPO" && git rev-parse --git-common-dir 2>/dev/null) && K="$(cd "$REPO" && cd "$G/.." && pwd)/build/tools/fcheck/kept"
mkdir -p "$K"
DATA=$(cat "$ROM" "$WORK/inputs"/*.bin | shasum)
trap 'pkill -f "$X/" 2>/dev/null; kill $(jobs -p) 2>/dev/null; exit 130' INT TERM
n=0; ran=0
part() {   # part <binary> <command>: the next section, from the store or run in the background
  local f k; f="$P/$(printf %03d $n)"; n=$((n+1))
  k=$( { shasum < "$X/$1"; echo "$DATA"; echo "$2"; } | shasum | cut -c1-24 ); echo "$k" > "$f.key"
  if [ $FULL = 0 ] && [ -f "$K/$k" ]; then cp "$K/$k" "$f"; return; fi
  ran=$((ran+1))
  eval "$2" > "$f" 2>&1 &
}
keep() { local f k; for f in "$P"/[0-9][0-9][0-9]; do k=$(cat "$f.key"); cp "$f" "$K/$k.$$" && mv "$K/$k.$$" "$K/$k"; done; }
cd "$WORK/inputs"
part xt_osc 'echo "== xt_osc"; "$X/xt_osc" "$ROM" | grep -v "^  "'
[ -x "$X/xt_cosm" ] && part xt_cosm 'echo "== xt_cosm"; "$X/xt_cosm" "$ROM" 3000 | grep -v "^  "'
[ -x "$X/xt_analog" ] && part xt_analog 'echo "== xt_analog"; "$X/xt_analog" "$ROM" 3000 | grep -v "^  "'
part xt_lfo 'echo "== xt_lfo"; "$X/xt_lfo" "$ROM" | tail -25'
part xt_env 'echo "== xt_env"; "$X/xt_env" "$ROM" conv | tail -3'
for c in "0 127 0 100 40 300 2" "10 60 64 30 50 400 2" "50 90 20 80 120 600 0" "127 127 127 127 200 900 2" "0 0 127 0 5 60 0" "30 70 90 50 80 500 2"; do
  part xt_env "echo \"env $c: \$(\"\$X/xt_env\" \"\$ROM\" $c | tail -1)\""; done
[ -x "$X/xt_tone" ] && part xt_tone 'echo "== xt_tone"; "$X/xt_tone" "$ROM" 2000 2024 "$WORK/inputs" | grep -v "^  "'
# The SOLO encode's two paths and a SOLO wave's own events, on every wave there is: the disk image's preset waves, the project's, the inputs (about a minute, 8 threads)
[ -x "$X/xt_tone" ] && part xt_tone 'echo "== xt_tone paths"; "$X/xt_tone" "$ROM" paths "$REPO"/analysis/projects/*/Wave/*.wav vocal.bin chord.bin sine.bin gated.bin drum.bin:2 w358.bin:2 enc_*.bin $(ls enc2_*.bin | sed "s/\$/:2/") 2>/dev/null | grep -v "^  "'
# solocmp <file> <channels> <R pitch ratio> <F formant ratio, 0 = none> <walk> <energy gain 0..1> <frames> [frame:walk ...] [frame:gGAIN ...] [robot] [loop:on,fwdMode,bwdMode,fs,fe,bs,be]
h='echo "== solocmp"; '
for spec in "vocal.bin 1 1.0 0 1.0 0 30000" "vocal.bin 1 1.5 1.25 1.0 0.25 30000" "vocal.bin 1 0.7 0.8 2.0 0.6 30000" "vocal.bin 1 1.0 0 -1.0 0 30000" \
            "vocal.bin 1 1.2 1.1 0.5 0.05 30000 4000:-1.0 9000:2.0" "w358.bin 2 1.0 0 1.0 0 20000" "w358.bin 2 0.8 1.5 1.5 0.4 20000" "drum.bin 2 1.0 0 1.0 0 40000" "drum.bin 2 0.8 1.5 1.5 0.4 40000" \
            "gated.bin 1 8 0 -0.5 0 60000" "gated.bin 1 32 0 -1.0 0 31500 16000:1.0" "gated.bin 1 32 0 -1.0 0 4300000 10464:-0.001" \
            "gated.bin 1 32 0 -1.0 0 31500 robot" "gated.bin 1 32 0 0.5 0 40000 10464:-0.25 robot" \
            "vocal.bin 1 1.0 1.0 0.5 0 40000 loop:1,1,0,3000,9000,0,0" "vocal.bin 1 1.3 0.9 1.0 0.1 60000 loop:1,2,0,3000,9000,0,0" "w358.bin 2 1.0 1.0 -1.0 0.3 40000 loop:1,0,2,0,0,2000,8000" \
            "vocal.bin 1 0.6 1.7 1.0 0 40000 loop:1,3,0,3000,9000,0,0 500:g0.5 8000:g0.1" "w358.bin 2 1.0 1.0 1.0 0 24000" "vocal.bin 1 4.0 1.0 1.0 1.0 30000" "vocal.bin 1 0.03 1.0 0.2 0 30000" "w358.bin 2 0.5 1.2 1.0 0.7 30000 robot" "gated.bin 1 1.5 1.3 1.0 0.4 40000" \
            "w358.bin 2 1.0 1.0 -1.0 0 40000 12000:1.0 loop:1,0,2,0,0,2000,8000" "vocal.bin 1 0.6 1.7 1.0 0 40000 20000:-1.0 loop:1,3,0,3000,9000,0,0" "w358.bin 2 1.0 1.0 -1.0 0 40000 20000:1.0 loop:1,0,3,0,0,2000,8000" \
            "w358.bin 2 1.0 1.0 -1.0 0 30000 loop:1,0,1,0,0,2000,13298" "w358.bin 2 1.0 1.0 1.0 0 30000 loop:1,1,0,9800,12000,0,0" "w358.bin 2 1.0 1.0 1.0 0 40000 14000:-1.0 loop:1,0,1,0,0,2000,8000" \
            "vocal.bin 1 1.0 1.0 0.1 0 60000 1000:-0.001 40000:1.0" "gated.bin 1 1.5 1.0 1.0 0 15800 14300:0.0625" "vocal.bin 1 0.002 1.0 0.2 0 150000"; do
  part xt_fw "${h}echo \"solo $spec: \$(\"\$X/xt_fw\" \"\$ROM\" solocmp $spec 2>/dev/null | tail -1)\""; h=""; done
# The SOLO encoder against the firmware's, whole records: the inputs above, stereo in its own mode (:2), and make_inputs.py's enc_* and enc2_*
h='echo "== xt_enc"; '
for f in vocal.bin chord.bin sine.bin gated.bin drum.bin:2 w358.bin:2 enc_*.bin; do
  part xt_enc "${h}\"\$X/xt_enc\" \"\$ROM\" cmp $f 2>/dev/null"; h=""; done
part xt_enc 'ls enc2_*.bin | sed "s/\$/:2/" | xargs -n 1 -P 8 "$X/xt_enc" "$ROM" cmp 2>/dev/null | LC_ALL=C sort'
# ... and against the records stored in the project's own waves
part xt_enc 'echo "== xtrecs"; "$PY" "$REPO/research/tools/xtrecs.py" "$X/xt_enc" "$ROM"'
# bandcmp <3 BACKING|5 ENSEMBLE> <file> <channels> <pitch> <walk> <frames> [frame:walk ...]
h='echo "== bandcmp"; '
for spec in "3 drum.bin 2 1.0 1.0 30000" "3 drum.bin 2 1.3 -1.0 30000" "5 chord.bin 1 0.8 1.5 30000" "5 chord.bin 1 1.0 0.0 20000 3000:1.0 8000:-0.5" \
            "5 vocal.bin 1 0.03 -0.2 3000 0:0.015" "3 drum.bin 2 0.5 -0.5 8000 0:0.3"; do
  part xt_fw "${h}echo \"band $spec: \$(\"\$X/xt_fw\" \"\$ROM\" bandcmp $spec 2>&1 | tail -1)\""; h=""; done
# V-Synth's files: the disk image's preset waves and patches against the firmware's own routines
[ -x "$X/xt_files" ] && part xt_files 'echo "== xt_files"; "$X/xt_files" "$ROM"'
# The effect chip against JE-8086's interpreter, on the XT's own programs, and the firmware's own numbers on it
[ -x "$X/xt_fx" ] && part xt_fx 'echo "== xt_fx"; "$X/xt_fx" "$ROM" 8192'
[ -x "$X/xt_fx" ] && part xt_fx '"$X/xt_fx" "$ROM" numbers'
# The effects' control side: the firmware's driver through the queue, against the same driver on the reference
[ -x "$X/xt_fx" ] && part xt_fx '"$X/xt_fx" "$ROM" control'
wait
cat "$P"/[0-9][0-9][0-9] > "$O/results.txt"
if [ $UPDATE = 1 ]; then cp "$O/results.txt" "$B"; keep; echo "fcheck: baseline updated ($(wc -l < "$B") lines)"; exit 0; fi
if diff -q "$B" "$O/results.txt" > /dev/null; then
  keep; echo "fcheck: as baseline ($(grep -c "different\|identical\|differ" "$O/results.txt") checks; $ran of $n sections run)"
else
  echo "fcheck: DIFFERS from baseline:"; diff "$B" "$O/results.txt" | grep "^[<>]" | head -20; exit 1
fi
