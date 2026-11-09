#!/bin/bash
# buildid_test.sh: plugin/BuildId.cmake on a small git repository of its own, through what a working day does to one.  A second or so.  In verify.sh.
source "$(dirname "$0")/env.sh"
T="$WORK/buildid_test"; rm -rf "${T:?}"; mkdir -p "$T/plugin/Source" "$T/research"
SCRIPT="$REPO/plugin/BuildId.cmake"
git -C "$T" init -q; git -C "$T" config user.email t@t; git -C "$T" config user.name t
n=0; fail=0
run() { cmake -DREPO="$1" -DOUT="$T.out" -P "$SCRIPT" || echo "(the script failed)"; sed -n 's/^#define XT_BUILD_ID "\(.*\)"$/\1/p' "$T.out"; }
say() { n=$((n+1)); [ "$2" = "$3" ] || { fail=$((fail+1)); echo "buildid_test: $1: got '$2', wanted '$3'"; }; }
differs() { n=$((n+1)); [ "$2" != "$3" ] || { fail=$((fail+1)); echo "buildid_test: $1: both '$2'"; }; }
sha() { git -C "$T" rev-parse --short=7 HEAD; }
commit() { git -C "$T" add -A; git -C "$T" commit -q -m "$1"; }
rm -f "$T.out"

echo a > "$T/plugin/Source/a.cpp"; echo d > "$T/research/doc.md"
commit "first, not a version"
say "a commit with no version before it" "$(run "$T")" "$(sha)"

commit_v() { echo "$1" >> "$T/plugin/Source/a.cpp"; commit "$1: something"; }
commit_v v7
say "a version" "$(run "$T")" "v7 $(sha)"
echo more > "$T/research/doc.md"
say "the docs edited: the same build" "$(run "$T")" "v7 $(sha)"
commit "notes, with no number"
say "a commit that isn't a version: the last version's number, its own hash" "$(run "$T")" "v7 $(sha)"

echo change >> "$T/plugin/Source/a.cpp"
one=$(run "$T")
case "$one" in "v7 $(sha)+"????) n=$((n+1)) ;; *) say "a source edited" "$one" "v7 $(sha)+abcd" ;; esac
say "the same edit again: the same build" "$(run "$T")" "$one"
echo other >> "$T/plugin/Source/a.cpp"
two=$(run "$T")
differs "a further edit reads differently" "$one" "$two"
git -C "$T" checkout -q -- plugin/Source/a.cpp
say "the edit undone: the commit's own build" "$(run "$T")" "v7 $(sha)"

echo new > "$T/plugin/Source/new.cpp"
three=$(run "$T")
differs "a new file counts" "$three" "v7 $(sha)"
echo newer > "$T/plugin/Source/new.cpp"
differs "and its contents" "$(run "$T")" "$three"
rm "$T/plugin/Source/new.cpp"
git -C "$T" rm -q --cached plugin/Source/a.cpp; rm "$T/plugin/Source/a.cpp"
differs "a file deleted counts" "$(run "$T")" "v7 $(sha)"
git -C "$T" reset -q --hard
say "and back" "$(run "$T")" "v7 $(sha)"

echo staged >> "$T/plugin/Source/a.cpp"; git -C "$T" add -A
differs "a change staged counts" "$(run "$T")" "v7 $(sha)"
git -C "$T" reset -q --hard

# The file is left alone when the id hasn't changed, and with it the time, so nothing relinks
run "$T" > /dev/null
echo '// the time as it was' >> "$T.out"; before=$(cat "$T.out")
run "$T" > /dev/null
say "the same id leaves the file alone" "$(cat "$T.out")" "$before"
commit_v v8
say "a new id rewrites it" "$(run "$T")" "v8 $(sha)"
n=$((n+1)); grep -q 'the time as it was' "$T.out" && { fail=$((fail+1)); echo "buildid_test: the old file survived a new id"; }
n=$((n+1)); grep -Eq '^#define XT_BUILD_TIME "[0-9]{2} [A-Z][a-z]{2} [0-9]{2}:[0-9]{2}"$' "$T.out" || { fail=$((fail+1)); echo "buildid_test: the time: $(tail -1 "$T.out")"; }

none=$(mktemp -d)        # outside this checkout, or git finds it
say "no git: still a build" "$(run "$none")" "unknown"
rm -rf "${none:?}" "${T:?}.out"

[ $fail = 0 ] && echo "build id's script: $n cases as they should be" || { echo "buildid_test: $fail of $n wrong"; exit 1; }
