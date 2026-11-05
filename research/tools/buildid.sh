#!/bin/bash
# buildid.sh [--installed]: which build each copy of the plugin is, against this checkout.  Each line: where, and the id it carries (plugin/BuildId.cmake says how to read one).
# With no argument, what verify.sh runs: the test binary and the AU and VST3 this checkout's build installed must carry this checkout's id, and that id must be what git says.
# --installed: only says what's there, every copy found, and fails on nothing: any checkout's build overwrites ~/Library's.
source "$(dirname "$0")/env.sh"
idOf() { LC_ALL=C grep -a -o 'V-Synth Emu build [^[:cntrl:]]*' "$1" 2>/dev/null | head -1 | sed 's/^V-Synth Emu build //'; }

now="$WORK/buildid_now.h"; rm -f "$now"
cmake -DREPO="$REPO" -DOUT="$now" -P "$REPO/plugin/BuildId.cmake" || { echo "build id: the script failed"; exit 1; }
here=$(sed -n 's/^#define XT_BUILD_ID "\(.*\)"$/\1/p' "$now")

AU="$HOME/Library/Audio/Plug-Ins/Components/V-Synth Emu.component/Contents/MacOS/V-Synth Emu"
VST3="$HOME/Library/Audio/Plug-Ins/VST3/V-Synth Emu.vst3/Contents/MacOS/V-Synth Emu"
SA="$REPO/build/plugin/VSynthEmu_artefacts/Release/Standalone/V-Synth Emu.app/Contents/MacOS/V-Synth Emu"
TEST="$REPO/build/plugin/VSynthEmuTest"

if [ "$1" = "--installed" ]; then
  echo "this checkout: $here"
  for f in "$AU" "$VST3" "/Library/Audio/Plug-Ins/Components/V-Synth Emu.component/Contents/MacOS/V-Synth Emu" \
           /Library/Audio/Plug-Ins/VST3/*/"V-Synth Emu.vst3/Contents/MacOS/V-Synth Emu" "/Library/Audio/Plug-Ins/VST3/V-Synth Emu.vst3/Contents/MacOS/V-Synth Emu" "$SA"; do
    [ -f "$f" ] || continue
    id=$(idOf "$f")
    echo "${id:-older than v150, no id}: $f"
  done
  exit 0
fi

# What git says, by another road than the script's
sha=$(git -C "$REPO" rev-parse --short=7 HEAD)
number=$(git -C "$REPO" log --format=%s | grep -E -m1 '^v[0-9]+: ' | sed 's/:.*//')
bad=""
case "$here" in "$number $sha"|"$number $sha+"*) ;; *) bad="$bad\n  this checkout says $here, git $number $sha" ;; esac
for f in "$TEST" "$AU" "$VST3"; do
  id=$(idOf "$f")
  [ "$id" = "$here" ] || bad="$bad\n  ${id:-no id} in $f"
done
if [ -n "$bad" ]; then echo -e "build id: not $here:$bad"; exit 1; fi
echo "build id: $here, in the test, the AU and the VST3"
