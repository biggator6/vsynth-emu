#!/bin/bash
# verify.sh [--full] [git rev]: the standard validation of a version, one line each —
# firmware checks against the baseline (--full: all of them run, none reused), A/B
# renders against a revision (default HEAD; settings from XT_* env), the plugin build,
# PluginTest (TEST_* env), the engine tests and auval.
source "$(dirname "$0")/env.sh"
D="$(dirname "$0")"; rc=0; F=""; BUILT=1
[ "$1" = "--full" ] && { F=--full; shift; }
bash "$D/fcheck.sh" $F || rc=1
bash "$D/ab.sh" "${1:-HEAD}" || rc=1
# A Makefile from before the target has no rule for it, and make does not regenerate for a target it does not know
(cd "$REPO" && { grep -q VSynthEmuVerify build/plugin/Makefile 2>/dev/null || SDKROOT="$SDK" cmake -S plugin -B build/plugin -DCMAKE_BUILD_TYPE=Release; } > "$WORK/build_plugin.log" 2>&1 \
  && SDKROOT="$SDK" cmake --build build/plugin --target VSynthEmuVerify -j 10 >> "$WORK/build_plugin.log" 2>&1) \
  && echo "plugin: built" || { echo "plugin: BUILD FAILED (see $WORK/build_plugin.log)"; rc=1; BUILT=0; }
[ $BUILT = 1 ] && { bash "$D/pt.sh" || rc=1; }     # without a build, the old binaries would pass
echo "engine tests: $("$WORK/fcheck/bin/engine_tests" 2>&1 | tail -1)"
[ $BUILT = 1 ] && echo "auval: $(auval -v aumu VsEm Ystu 2>&1 | grep -o "AU VALIDATION SUCCEEDED\|FAIL.*" | head -1)"
exit $rc
