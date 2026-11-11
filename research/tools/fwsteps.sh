#!/bin/bash
# fwsteps.sh [every]: what each firmware call the plugin makes takes, against its machine's allowance (fwsteps.cpp;
# plugin/Source/XtFwMachine.h, v175).  Builds fwsteps against a copy of Sh4Interp.cpp that counts each call
# (fwsteps_count.py, into $WORK/fwsteps; plugin/Source is untouched) and runs it on the ROM, the disk image beside it
# and analysis/projects.  It encodes every 8th preset wave and the six longest by default (about 6 minutes); 1, all.
set -euo pipefail
source "$(dirname "$0")/env.sh"
S="$REPO/plugin/Source"
T="$REPO/research/tools"
O="$WORK/fwsteps"
mkdir -p "$O"
"$PY" "$T/fwsteps_count.py" "$S/Sh4Interp.cpp" "$O/Sh4Interp_counted.cpp"
"$TC/clang++" -isysroot "$SDK" -std=c++20 -O2 -I"$S" "$T/fwsteps.cpp" "$O/Sh4Interp_counted.cpp" \
    "$S/XtFirmware.cpp" "$S/XtFwMachine.cpp" "$S/XtFiles.cpp" "$S/XtPatchStore.cpp" "$S/XtWaveFile.cpp" \
    "$S/XtMultiBandPlayer.cpp" \
    "$S/XtSoloVoice.cpp" "$S/XtSoloEncoder.cpp" "$S/XtDiskImage.cpp" -o "$O/fwsteps"
"$O/fwsteps" "$ROM" "$(dirname "$ROM")/V-SynthXTDiskImage.bin.0.bin" "$REPO/analysis/projects" "${1:-8}"
