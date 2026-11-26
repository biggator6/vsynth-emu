#!/bin/bash
# buildid_test.sh: plugin/BuildId.cmake on a small git repository of its own, through what a working day does to one, then buildid.sh on it as a
# checkout, with planted copies of its build and of ~/Library's.  A few seconds.  In verify.sh.
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

# buildid.sh, verify.sh's line, on this repository as a checkout: its own build's test, AU and VST3 hold it to its id,
# and the folder the builds install into (PLUGINS, ~/Library's stand-in) is only named.  Another checkout's build
# installing between a run's build and its check must not fail it (2026-11-22, workflow-bugs.md).
mkdir -p "$T/research/tools"
: > "$T/research/tools/env.sh"
cp "$SCRIPT" "$T/plugin/BuildId.cmake"
commit "the tools, not a version"
id="v8 $(sha)"
B="$T/build/plugin"
A="$B/VSynthEmu_artefacts/Release"
P="$T/plugins"
other="v330 dbddaec"
carry() {
    mkdir -p "$(dirname "$1")"
    printf 'x\0V-Synth Emu build %s\0y' "$2" > "$1"
}
F="$B/VSynthEmuFx_artefacts/Release"   # v462: the effect build's, beside the instrument's
FX="V-Synth Emu FX"
NAMES=("V-Synth Emu" "$FX")   # the products, and where this build makes each
MADE=("$A" "$F")
# A bundle's binary in a folder, the instrument's unless another product is named
au() { echo "$1/${2:-V-Synth Emu}.component/Contents/MacOS/${2:-V-Synth Emu}"; }
vst3() { echo "$1/${2:-V-Synth Emu}.vst3/Contents/MacOS/${2:-V-Synth Emu}"; }
# This build's copies: the test with the first id, both products' AUs with the second, their VST3s with the third
own() {
    carry "$B/VSynthEmuTest" "$1"
    for i in 0 1; do
        carry "$(au "${MADE[i]}/AU" "${NAMES[i]}")" "$2"
        carry "$(vst3 "${MADE[i]}/VST3" "${NAMES[i]}")" "$3"
    done
}
# ~/Library's: both products' AUs with the first id, their VST3s with the second
installed() {
    for name in "${NAMES[@]}"; do
        carry "$(au "$P/Components" "$name")" "$1"
        carry "$(vst3 "$P/VST3" "$name")" "$2"
    done
}
# expect <case> <exit status> <text its line must hold>
expect() {
    local o st
    n=$((n+1))
    o=$(REPO="$T" PLUGINS="$P" bash "$(dirname "$0")/buildid.sh" 2>&1)
    st=$?
    [ "$st" = "$2" ] && [[ "$o" == *"$3"* ]] && return
    fail=$((fail+1))
    echo "buildid_test: $1: exit $st, wanted $2 and '$3'; it said: $o"
}
own "$id" "$id" "$id"
installed "$id" "$id"
expect "this build installed" 0 "installed: AU this build's, VST3 this build's"
installed "$other" "v394 1234567+abcd"
expect "another checkout's build installed since: named, not failed" 0 \
    "installed: AU $other (another build's), VST3 v394 1234567+abcd (another build's)"
rm -rf "${P:?}"
expect "nothing installed" 0 "installed: no AU, no VST3"
# This build's own copies wrong, with ~/Library's right: the line reads what was built here
installed "$id" "$id"
own "$id" "$other" "$id"
expect "a wrong id in this build's AU" 1 "$other in $A/AU/"
own "$id" "$id" "$other"
expect "a wrong id in this build's VST3" 1 "$other in $A/VST3/"
own "$other" "$id" "$id"
expect "a wrong id in this build's test" 1 "$other in $B/VSynthEmuTest"
own "$id" "$id" "$id"
rm -rf "${A:?}/AU"
expect "this build's AU missing" 1 "no id in $A/AU/"
# v462: the effect build's copies held to the same id
own "$id" "$id" "$id"
carry "$(au "$F/AU" "$FX")" "$other"
expect "a wrong id in the effect build's AU" 1 "$other in $F/AU/"
own "$id" "$id" "$id"
rm -rf "${F:?}/VST3"
expect "the effect build's VST3 missing" 1 "no id in $F/VST3/"
own "$id" "$id" "$id"
expect "the effect build's installed copies named" 0 "FX AU this build's, FX VST3 this build's"

[ $fail = 0 ] && echo "build id's script: $n cases as they should be" || { echo "buildid_test: $fail of $n wrong"; exit 1; }
