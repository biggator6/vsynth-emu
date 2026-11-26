#!/bin/bash
# verify_test.sh: verify.sh fails the run when a line it prints fails (R4 of .claude/claude-packs-review.md).  A stub
# auval first on PATH fails; the run must end non-zero, the auval line of each AU (the instrument's aumu and, v461, the
# effect build's aumf) saying so.  It runs the whole of verify.sh (a few minutes), so run it after a verify.sh that
# passed: then the stub is the one thing that fails.  Not in verify.sh.
set -euo pipefail
source "$(dirname "$0")/env.sh"
T=$(mktemp -d)
trap 'rm -rf "${T:?}"' EXIT
mkdir -p "$T/bin"
printf '#!/bin/sh\necho "FAIL: a stub auval, for verify_test.sh"\nexit 1\n' > "$T/bin/auval"
chmod +x "$T/bin/auval"
set +e
out=$(PATH="$T/bin:$PATH" bash "$(dirname "$0")/verify.sh" 2>&1)
st=$?
set -e
ok=1
for kind in aumu aumf; do
    line=$(echo "$out" | grep "^auval $kind:" || true)
    [[ "$line" == *"FAIL: a stub auval"* ]] || { ok=0; echo "verify test: auval $kind's line \"$line\""; }
done
if [ "$st" -ne 0 ] && [ "$ok" = 1 ]; then
    echo "verify test: a failed auval fails the run, for both AUs"
    exit 0
fi
echo "verify test: verify.sh exited $st"
exit 1
