#!/bin/bash
# gitlinks_test.sh [--plants]: a fresh clone can fill each of its gitlinks (JUCE) with `git submodule update --init`,
# because .gitmodules names where each comes from (.claude/fix-plan.md, section 9; v184).  On a --shared clone of HEAD
# with no files checked out, given this checkout's .gitmodules: `git submodule init` must register a URL for every
# gitlink.  It fetches nothing.  A second or so; in verify.sh.  --plants: with an empty .gitmodules, every gitlink must
# be found wanting.
set -euo pipefail
source "$(dirname "$0")/env.sh"
T=$(mktemp -d)
trap 'rm -rf "${T:?}"' EXIT
git clone -q --shared --no-checkout "$REPO" "$T/clone"
git -C "$T/clone" reset -q                                    # the index, for the gitlinks; no files
if [ "${1:-}" = --plants ]; then
    : > "$T/clone/.gitmodules"
elif [ -f "$REPO/.gitmodules" ]; then
    cp "$REPO/.gitmodules" "$T/clone/.gitmodules"
fi
links=0
wrong=0
while read -r mode _ _ path; do
    [ "$mode" = 160000 ] || continue
    links=$((links + 1))
    if ! git -C "$T/clone" submodule init -- "$path" > /dev/null 2>&1; then
        wrong=$((wrong + 1))
        echo "gitlinks_test: $path is a gitlink with no URL in .gitmodules"
    fi
done < <(git -C "$T/clone" ls-files -s)
if [ "${1:-}" = --plants ]; then
    if [ "$links" -gt 0 ] && [ "$wrong" = "$links" ]; then
        echo "gitlinks_test plants: an empty .gitmodules leaves all $links gitlinks wanting, as it should"
        exit 0
    fi
    echo "gitlinks_test plants: an empty .gitmodules left $wrong of $links gitlinks wanting"
    exit 1
fi
if [ "$links" = 0 ] || [ "$wrong" != 0 ]; then
    echo "gitlinks_test: $wrong of $links gitlinks with no URL"
    exit 1
fi
echo "gitlinks: $links of $links with their URL in .gitmodules"
