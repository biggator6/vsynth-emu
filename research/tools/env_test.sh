#!/bin/bash
# env_test.sh [--plants]: env.sh finds its checkout from bash and from zsh, wherever it's sourced from, and refuses a
# REPO that isn't one rather than making build/tools there (.claude/fix-plan.md, section 9; v178).  It runs on a copy of
# env.sh in a scratch tree, the copy's checkout three folders down, so a REPO found wrong lands inside the tree too and
# the last case finds its build folder.  A second or so; in verify.sh.  --plants: each error in PLANTS, put in the copy
# one at a time, must make a case wrong.
set -euo pipefail
SRC="$(cd "$(dirname "$0")" && pwd)/env.sh"
T=$(cd "$(mktemp -d)" && pwd -P)   # physical, as a shell started with no PWD finds its own
trap 'rm -rf "${T:?}"' EXIT
C="$T/x/y/repo"
E="$C/research/tools/env.sh"

# The shell and its options | where it runs, in the copy's checkout | the path it sources | REPO given, if any | the
# REPO it must have, or refused.  Each starts with an empty environment but that REPO, so nothing else of env.sh's is set.
CASES=(
    "bash --norc|sub|$E||$C"
    "bash --norc|.|research/tools/env.sh||$C"
    "zsh -f|.|research/tools/env.sh||$C"
    "zsh -f|sub|$E||$C"
    "zsh -f -o POSIX_ARGZERO|sub|$E||refused"
    "bash --norc|sub|$E|$C|$C"
    "bash --norc|sub|$E|$T/x|refused"
)
# name | text in env.sh | what it becomes
PLANTS=(
    'bash-only|${BASH_SOURCE[0]:-$0}|${BASH_SOURCE[0]}'
    'no-refusal|if [ ! -f "$REPO/research/tools/env.sh" ]; then|if false; then'
)

# The cases on env.sh file $1, copied into a fresh tree: a line for each that's wrong, their count in `wrong`
run() {
    rm -rf "${T:?}/x"
    mkdir -p "$C/research/tools" "$C/sub"
    cp "$1" "$E"
    wrong=0
    local row shell dir path given want got stray
    local -a cmd
    for row in "${CASES[@]}"; do
        IFS='|' read -r shell dir path given want <<< "$row"
        read -ra cmd <<< "$shell"
        # an empty REPO is as none: env.sh takes ${REPO:-...}
        got=$(cd "$C/$dir" && env -i HOME="$HOME" PATH="$PATH" REPO="$given" "${cmd[@]}" \
            -c 'source "$1" && echo "$REPO"' env_test "$path" 2> /dev/null) || got=refused
        if [ "$got" != "$want" ]; then
            wrong=$((wrong + 1))
            echo "env_test: $shell in $dir, sourcing ${path#"$T"}, REPO given '${given#"$T"}': REPO ${got#"$T"}," \
                "wanted ${want#"$T"}"
        fi
    done
    stray=$(find "$T" -type d -name build ! -path "$C/build")   # a REPO found wrong makes its build/tools there
    if [ -n "$stray" ]; then
        wrong=$((wrong + 1))
        stray=${stray//"$T"/}
        echo "env_test: build folders outside the checkout: ${stray//$'\n'/, }"
    fi
}

if [ "${1:-}" != --plants ]; then
    run "$SRC"
    if [ "$wrong" != 0 ]; then
        echo "env_test: $wrong of $((${#CASES[@]} + 1)) wrong"
        exit 1
    fi
    echo "env.sh: $((${#CASES[@]} + 1)) cases as they should be"
    exit 0
fi

run "$SRC"
if [ "$wrong" != 0 ]; then
    echo "env_test plants: env.sh as it is fails, so no plant would mean anything"
    exit 1
fi
missed=0
for plant in "${PLANTS[@]}"; do
    IFS='|' read -r name old new <<< "$plant"
    text=$(cat "$SRC")
    if [[ "$text" != *"$old"* ]]; then
        echo "ERROR  $name: its text isn't in env.sh"
        missed=$((missed + 1))
        continue
    fi
    printf '%s\n' "${text/"$old"/$new}" > "$T/planted.sh"
    run "$T/planted.sh" > /dev/null
    if [ "$wrong" = 0 ]; then
        echo "MISSED $name"
        missed=$((missed + 1))
    else
        echo "caught $name: $wrong wrong"
    fi
done
echo "env_test plants: ${#PLANTS[@]} planted, $missed missed"
[ "$missed" = 0 ]
