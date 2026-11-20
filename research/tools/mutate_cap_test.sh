#!/bin/bash
# mutate_cap_test.sh (v312): mutate.sh's capped (mutate_cap.sh) as mutate.sh reads a run: a run growing past MUT_MEM_MB
# is killed and reads as died; one under the cap, one exiting with its own status and one hanging past MUT_TIMEOUT read
# as before.  Its allocator touches 64 MB every 50 ms and stops itself at 3 GB, so a cap that failed could not take
# the machine.  Each line says what came; the last says whether all came as asked.  When mutate_cap.sh changes.
set -euo pipefail
source "$(dirname "$0")/env.sh"
source "$(dirname "$0")/mutate_cap.sh"
GROW='import sys, time
step, most = int(sys.argv[1]), int(sys.argv[2])
held = []
for n in range(step, most + 1, step):
    held.append(b"\x01" * (step << 20))
    print("held", n, "MB", flush=True)
    time.sleep(0.05)'
OUT=$(mktemp)
trap 'rm -f "$OUT"' EXIT
said() {   # said <name> <died: yes|no> <command...>: run as mutate.sh runs a mutant, read as it reads the status
    local name=$1 wanted=$2
    shift 2
    local st=0 died=no
    capped "$@" > "$OUT" || st=$?
    [ "$st" = 124 ] && st=$((128 + 14))
    if [ "$st" -ge 128 ]; then
        died=yes
        echo "$name: the run died, signal $((st - 128)) (last: $(tail -1 "$OUT"))"
    else
        echo "$name: status $st (last: $(tail -1 "$OUT"))"
    fi
    if [ "$died" != "$wanted" ]; then
        echo "mutate_cap_test: FAILED at $name"
        exit 1
    fi
}
MUT_MEM_MB=512 said "past the cap" yes "$PY" -c "$GROW" 64 3072
MUT_MEM_MB=512 said "under the cap" no "$PY" -c "$GROW" 64 256
said "its own status" no sh -c 'echo three; exit 3'
MUT_TIMEOUT=1 said "past the time" yes sleep 5
echo "mutate_cap_test: all as asked"
