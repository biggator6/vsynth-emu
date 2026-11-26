#!/bin/bash
# build_vc2patchwrite.sh [out]: vc2patchwrite (research/tools/vc2patchwrite.cpp), the VC-2 captures' card patches by
# rom.4's own save (v505, Vc2PatchSave.cpp), from the working tree's plugin/Source into out (default
# $WORK/vc2/vc2patchwrite, where vc2capture.py looks).  About 10 s.  Kept out of build.sh, which holds the harnesses.
set -euo pipefail
source "$(dirname "$0")/env.sh"
OUT="${1:-$WORK/vc2/vc2patchwrite}"
mkdir -p "$(dirname "$OUT")"
rm -f "$OUT"
cd "$REPO/plugin/Source"
"$TC/clang++" -isysroot "$SDK" -std=c++20 -O2 -I. "$REPO/research/tools/vc2patchwrite.cpp" Vc2PatchSave.cpp \
    Vc2PatchFile.cpp Vc2PatchStore.cpp XtPatchStore.cpp Vc2Params.cpp XtFxValueText.cpp XtFirmware.cpp XtFwMachine.cpp \
    XtMultiBandPlayer.cpp XtSoloVoice.cpp Sh4Interp.cpp Sh4Setup.cpp -o "$OUT"
echo "vc2patchwrite: built, $OUT"
