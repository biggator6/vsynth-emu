#!/bin/bash
# fxplay_test.sh: fxplay against the runs it plays, with no hardware (about a minute).  It builds fxplay into
# build/tools/fxplay/, then:
#   - every byte of the effects' runs and the VC-2's (their Bank Select and Program Change too) arrives at a virtual
#     output (--test), the cues sent as nothing;
#   - the input of each VC-2 run, rendered offline through fxplay's own AudioQueue (--test-audio), is sample for
#     sample the run's input as vc2capture.py --input-wavs writes it, by its own code (the cues' times and lengths);
#   - a run whose cue isn't probe:<seconds> is refused, saying so.
# Needs the user's rom.2 and rom.4 (firmware/unpacked/, or ROM2= and ROM4=), build.sh's vc2patchwrite and .venv's
# numpy; PY= another python.
# Exits non-zero on any difference.
set -euo pipefail
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
PY="${PY:-$REPO/.venv/bin/python3}"
ROM2="${ROM2:-$REPO/firmware/unpacked/V-SynthXT.rom.2.bin}"
ROM4="${ROM4:-$REPO/firmware/unpacked/V-SynthXT.rom.4.bin}"
WORK="$REPO/build/tools/fxplay_test"
BIN="$REPO/build/tools/fxplay/fxplay"
rm -rf "${WORK:?}"
mkdir -p "$WORK/fx" "$WORK/vc2" "$(dirname "$BIN")"
clang++ -std=c++17 -O2 -Wall -Werror "$REPO/research/tools/fxplay.cpp" -framework CoreMIDI -framework CoreFoundation \
    -framework AudioToolbox -framework CoreAudio -o "$BIN"
"$PY" "$REPO/research/tools/fxcapture.py" --rom "$ROM2" --out "$WORK/fx" > /dev/null
if ! "$PY" "$REPO/research/tools/vc2capture.py" --rom4 "$ROM4" --out "$WORK/vc2" --input-wavs > "$WORK/vc2.txt"; then
    # vc2capture.py needs rom.4 and build.sh's vc2patchwrite, which writes the card's patches the VC-2 runs select
    echo "fxplay_test: WRONG, no VC-2 runs to test: $(head -c 300 "$WORK/vc2.txt")…"
    exit 1
fi
failed=0
for run in "$WORK"/fx/*.mid "$WORK"/vc2/*.mid; do
    if ! "$BIN" --test "$run"; then
        echo "WRONG: $(basename "$run") did not arrive identical"
        failed=1
    fi
done
for n in 1 2 3 4; do
    "$BIN" --test-audio "$WORK/vc2/vc2_run_$n.mid" "$WORK/vc2/vc2probe.wav" "$WORK/rendered_$n.wav" > /dev/null
    if ! "$PY" - "$WORK/rendered_$n.wav" "$WORK/vc2/vc2_run_${n}_in.wav" <<'EOF'
import sys
import numpy as np
def frames(p):
    with open(p, 'rb') as f:
        b = f.read()
    i = b.index(b'data') + 8
    v = np.frombuffer(b[i:i + (len(b) - i) // 6 * 6], dtype=np.uint8).reshape(-1, 3).astype(np.int32)
    return ((v[:, 0] | v[:, 1] << 8 | v[:, 2] << 16) ^ 0x800000) - 0x800000
got, want = frames(sys.argv[1]), frames(sys.argv[2])
n = len(got)
bad = int(np.count_nonzero(got != want[:n])) + int(np.count_nonzero(want[n:]))
print('%s: %d samples rendered, %d different from vc2capture.py\'s input' % (sys.argv[1].split('/')[-1], n, bad))
sys.exit(1 if bad or n == 0 else 0)
EOF
    then
        failed=1
    fi
done
"$PY" - "$WORK/vc2/vc2_test.mid" "$WORK/bad_cue.mid" <<'EOF'
import sys
with open(sys.argv[1], 'rb') as f:
    b = f.read()
with open(sys.argv[2], 'wb') as f:
    f.write(b.replace(b'probe:16.8', b'probe:abcd'))
EOF
if "$BIN" --test-audio "$WORK/bad_cue.mid" "$WORK/vc2/vc2probe.wav" "$WORK/bad.wav" 2> "$WORK/bad.txt" \
        || ! grep -q 'not probe:<seconds>' "$WORK/bad.txt"; then
    echo "WRONG: a cue reading probe:abcd was not refused: $(cat "$WORK/bad.txt")"
    failed=1
else
    echo "a cue reading probe:abcd: refused"
fi
rm -rf "${WORK:?}"
[ "$failed" = 0 ] && echo "fxplay_test: all good" || echo "fxplay_test: WRONG"
exit "$failed"
