#!/bin/bash
# fxthreads.sh [events] [seed]: xt_fx's part with threads, under the thread sanitizer (the queue between the driver and the audio thread).  About half a minute with the build.  Prints the result and any report's first lines; says "no races" or "RACES".
source "$(dirname "$0")/env.sh"
S="$REPO/plugin/Source"; O="$WORK/fxthreads"; mkdir -p "$O"
"$TC/clang++" -isysroot "$SDK" -std=c++20 -O1 -g -fsanitize=thread -DXT_FX_MAIN -I"$REPO/research/third_party/gearmulator_esp" -I"$S" \
  "$S/XtFxChip.cpp" "$S/XtFxControl.cpp" "$S/Sh4Interp.cpp" -o "$O/xt_fx_tsan" 2>&1 | grep -E " error"
TSAN_OPTIONS="halt_on_error=0" "$O/xt_fx_tsan" "$ROM" threads "${1:-60}" "${2:-2024}" > "$O/out.txt" 2>&1
grep -E "different" "$O/out.txt"
n=$(grep -c "WARNING: ThreadSanitizer" "$O/out.txt")
if [ "$n" = 0 ]; then echo "fxthreads: no races"; else echo "fxthreads: RACES ($n reports)"; grep -A12 -m1 "WARNING: ThreadSanitizer" "$O/out.txt" | cut -c1-140; exit 1; fi
