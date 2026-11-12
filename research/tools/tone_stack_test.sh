#!/bin/bash
# tone_stack_test.sh [cases] [seed]: xt_tone's cases must not depend on bytes nobody wrote.  build.sh builds every harness
# twice, plain (as fcheck's, locals holding what the stack held) and with locals starting as a pattern (XT_STACK_INIT,
# clang's -ftrivial-auto-var-init), and the two xt_tones, nothing planted, must say the same, line for line.  Not zero
# against pattern: each writes every local alike, so two objects' padding is the same bytes in either build, and a compare of
# padding reads "equal" in both (v189's first run: 2000 cases at seed 24, the same, on the fault).  Until v189 they didn't: the one-word live edit compared the
# controls' padding, so how often it drew again, and every case after, followed the stack (workflow-bugs.md).  A few
# minutes; when XtToneCheck.cpp or the controls change.  Seed 24 by default, where v188's two builds part at its 353rd case.
set -eo pipefail
source "$(dirname "$0")/env.sh"
CASES="${1:-2000}"; SEED="${2:-24}"; T="$WORK/stack"; mkdir -p "$T"
pids=()
trap 'kill "${pids[@]}" 2>/dev/null || true' EXIT INT TERM   # stopped, the builds and runs it started stop too
[ -f "$ROM" ] || { echo "tone_stack_test: no ROM at $ROM"; exit 2; }
for init in plain pattern; do
    rm -f "$T/$init/xt_tone"
    XT_STACK_INIT=${init#plain} bash "$REPO/research/tools/build.sh" "$REPO/plugin/Source" "$T/$init" > "$T/build_$init.txt" 2>&1 &
    pids+=("$!")
done
wait
for init in plain pattern; do
    [ -x "$T/$init/xt_tone" ] || { echo "tone_stack_test: xt_tone didn't build with $init:"; head -5 "$T/build_$init.txt"; exit 2; }
    "$T/$init/xt_tone" "$ROM" "$CASES" "$SEED" "$WORK/inputs" > "$T/$init.txt" 2>&1 &
    pids+=("$!")
done
wait
grep -q ' different' "$T/plain.txt" || { echo "tone_stack_test: xt_tone said nothing:"; tail -3 "$T/plain.txt"; exit 2; }
if diff "$T/plain.txt" "$T/pattern.txt" > "$T/diff.txt"; then
    echo "tone_stack_test: the same, $CASES cases at seed $SEED"
    exit 0
fi
echo "tone_stack_test: xt_tone follows the stack's bytes ($CASES cases at seed $SEED):"
head -8 "$T/diff.txt"
exit 1
