#!/bin/bash
# fcheck_keys_test.sh: a kept section of fcheck.sh comes back only while nothing it reads has changed (R5 of
# .claude/claude-packs-review.md).  In a scratch tree with a copy of the tools, the baseline, the project's waves, the
# inputs and this checkout's kept sections, fcheck runs on this checkout's plugin/Source: once as it is, again after
# xtrecs.py gains a comment (its section must run), and again after a byte of a project wave changes (the sections
# reading the waves must run, and differ).  Three fcheck runs, a few minutes.  Not in verify.sh.
set -euo pipefail
source "$(dirname "$0")/env.sh"
G=$(cd "$REPO" && git rev-parse --git-common-dir)
KEPT="$(cd "$REPO" && cd "$G/.." && pwd)/build/tools/fcheck/kept"
T=$(mktemp -d)
trap 'rm -rf "${T:?}"' EXIT
mkdir -p "$T/research" "$T/analysis/projects" "$T/build/tools/fcheck"
cp -R "$REPO/research/tools" "$T/research/tools"
ln -s "$REPO/research/third_party" "$T/research/third_party"
cp -R "$REPO/analysis/projects/Emu_test.prj" "$T/analysis/projects/"
cp -R "$WORK/inputs" "$T/build/tools/inputs"
cp -R "$KEPT" "$T/build/tools/fcheck/kept"
run() {
    REPO="$T" WORK="$T/build/tools" ROM="$ROM" PY="$PY" bash "$T/research/tools/fcheck.sh" "$REPO/plugin/Source" 2>&1 || true
}
ran() { echo "$1" | grep -o '[0-9]* of [0-9]* sections run' | cut -d' ' -f1; }
failed=0
first=$(run)
if ! echo "$first" | grep -q '^fcheck: as baseline'; then
    echo "fcheck keys test: the scratch tree's first run isn't as baseline: $(echo "$first" | tail -3)"
    exit 1
fi
printf '\n# a comment, from fcheck_keys_test.sh\n' >> "$T/research/tools/xtrecs.py"
second=$(run)
n=$(ran "$second")
if [ "${n:-0}" -lt 1 ]; then
    echo "fcheck keys test: after xtrecs.py changed, ${n:-no} sections ran: $(echo "$second" | tail -1)"
    failed=1
fi
waves=("$T"/analysis/projects/Emu_test.prj/Wave/*.wav)
size=$(wc -c < "${waves[0]}")
printf '\x55' | dd of="${waves[0]}" bs=1 seek=$((size / 2)) conv=notrunc 2> /dev/null
third=$(run)
n=$(ran "$third")
if ! echo "$third" | grep -q 'DIFFERS' && [ "${n:-0}" -lt 1 ]; then
    echo "fcheck keys test: after a project wave changed, nothing ran again: $(echo "$third" | tail -1)"
    failed=1
fi
if [ "$failed" = 0 ]; then
    echo "fcheck keys test: a changed script and a changed wave both run again"
fi
exit "$failed"
