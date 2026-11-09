#!/bin/bash
# verify_test.sh: verify.sh fails the run when a line it prints fails (R4 of .claude/claude-packs-review.md).  A stub
# auval first on PATH fails; the run must end non-zero, its auval line saying so.  It runs the whole of verify.sh (a few
# minutes), so run it after a verify.sh that passed: then the stub is the one thing that fails.  Not in verify.sh.
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
line=$(echo "$out" | grep '^auval:' || true)
if [ "$st" -ne 0 ] && [[ "$line" == *"FAIL: a stub auval"* ]]; then
    echo "verify test: a failed auval fails the run"
    exit 0
fi
echo "verify test: verify.sh exited $st, its auval line \"$line\""
exit 1
