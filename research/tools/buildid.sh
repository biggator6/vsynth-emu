#!/bin/bash
# buildid.sh [--installed]: which build each copy of the plugin is, against this checkout.  Each line: where, and the id it carries (plugin/BuildId.cmake says how to read one).
# With no argument, what verify.sh runs: this checkout's own build, the test binary and the AU and VST3 bundles in build/plugin's artefacts (what its build
# copies into ~/Library), must carry this checkout's id, and that id must be what git says.  ~/Library's AU and VST3 are then named by their ids, and fail
# nothing: any checkout's build overwrites them, and another one's may land between this run's build and this check.
# --installed: only says what's there, every copy found, and fails on nothing.
# PLUGINS: the folder the builds install into (default ~/Library's), a stand-in for buildid_test.sh.
source "$(dirname "$0")/env.sh"
idOf() { LC_ALL=C grep -a -o 'V-Synth Emu build [^[:cntrl:]]*' "$1" 2>/dev/null | head -1 | sed 's/^V-Synth Emu build //'; }

now="$WORK/buildid_now.h"; rm -f "$now"
cmake -DREPO="$REPO" -DOUT="$now" -P "$REPO/plugin/BuildId.cmake" || { echo "build id: the script failed"; exit 1; }
here=$(sed -n 's/^#define XT_BUILD_ID "\(.*\)"$/\1/p' "$now")

PLUGINS="${PLUGINS:-$HOME/Library/Audio/Plug-Ins}"
AU="$PLUGINS/Components/V-Synth Emu.component/Contents/MacOS/V-Synth Emu"
VST3="$PLUGINS/VST3/V-Synth Emu.vst3/Contents/MacOS/V-Synth Emu"
BUILT="$REPO/build/plugin/VSynthEmu_artefacts/Release"     # JUCE's <target>_artefacts/<config>: verify.sh configures Release
BUILT_AU="$BUILT/AU/V-Synth Emu.component/Contents/MacOS/V-Synth Emu"
BUILT_VST3="$BUILT/VST3/V-Synth Emu.vst3/Contents/MacOS/V-Synth Emu"
SA="$BUILT/Standalone/V-Synth Emu.app/Contents/MacOS/V-Synth Emu"
# v462: the effect build's (plugin/FxPlugin.cmake), built and installed beside the instrument's
FX_AU="$PLUGINS/Components/V-Synth Emu FX.component/Contents/MacOS/V-Synth Emu FX"
FX_VST3="$PLUGINS/VST3/V-Synth Emu FX.vst3/Contents/MacOS/V-Synth Emu FX"
BUILT_FX="$REPO/build/plugin/VSynthEmuFx_artefacts/Release"
BUILT_FX_AU="$BUILT_FX/AU/V-Synth Emu FX.component/Contents/MacOS/V-Synth Emu FX"
BUILT_FX_VST3="$BUILT_FX/VST3/V-Synth Emu FX.vst3/Contents/MacOS/V-Synth Emu FX"
TEST="$REPO/build/plugin/VSynthEmuTest"

if [ "$1" = "--installed" ]; then
  echo "this checkout: $here"
  sys=/Library/Audio/Plug-Ins
  for f in "$AU" "$VST3" "$sys/Components/V-Synth Emu.component/Contents/MacOS/V-Synth Emu" \
           "$sys"/VST3/*/"V-Synth Emu.vst3/Contents/MacOS/V-Synth Emu" \
           "$sys/VST3/V-Synth Emu.vst3/Contents/MacOS/V-Synth Emu" "$SA" "$FX_AU" "$FX_VST3"; do
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
for f in "$TEST" "$BUILT_AU" "$BUILT_VST3" "$BUILT_FX_AU" "$BUILT_FX_VST3"; do
  id=$(idOf "$f")
  [ "$id" = "$here" ] || bad="$bad\n  ${id:-no id} in $f"
done
if [ -n "$bad" ]; then echo -e "build id: not $here:$bad"; exit 1; fi

# ~/Library's, named: this build's, or whose they are
installed=""
for kind in AU VST3 FX_AU FX_VST3; do
  f=${!kind}
  name=${kind/_/ }
  if [ ! -f "$f" ]; then
    installed="$installed, no $name"
    continue
  fi
  id=$(idOf "$f")
  if [ "$id" = "$here" ]; then
    installed="$installed, $name this build's"
  else
    installed="$installed, $name ${id:-older than v150, no id} (another build's)"
  fi
done
echo "build id: $here, in the test, the AU and the VST3 built here, the effect build's too; installed:${installed#,}"
