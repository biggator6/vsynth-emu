#!/bin/bash
# pt.sh [note]: PluginTest (the processor end to end against XtSynth at 48 kHz)
# on the four inputs; TEST_* settings pass through the environment.  Needs
# build/plugin/VSynthEmuTest (cmake --build build/plugin --target VSynthEmuTest).
# One line: "n/4 bit-identical", or each failing run.
source "$(dirname "$0")/env.sh"
N=${1:-60}; ok=0; fail=""
for spec in "analysis/test_files/raw/chord_Cmaj_original.wav|ensemble" "analysis/test_files/raw/drum_hit_original.wav|backing" \
            "analysis/test_files/raw/solo_male_aah_raw.wav|solo" "analysis/projects/Emu_test.prj/Wave/358 Synth002.wav|solo"; do
  IFS='|' read f m <<< "$spec"
  out=$(cd "$REPO" && build/plugin/VSynthEmuTest "$ROM" "$f" $N $m 2>&1); rc=$?
  if [ $rc = 0 ] && echo "$out" | grep -q "plugin - XtSynth| = 0 (first at frame -1)" && echo "$out" | grep -q "repeatable: yes"; then ok=$((ok+1))
  else fail="$fail\n  $(basename "$f") [$m] exit $rc: $(echo "$out" | grep -E "^rendered|repeatable|rejected|failed" | tr '\n' ' ')"; fi
done
echo -e "plugintest: $ok/4 bit-identical$fail"; [ $ok = 4 ]
