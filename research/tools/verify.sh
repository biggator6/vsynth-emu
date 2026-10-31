#!/bin/bash
# verify.sh [git rev]: the standard validation of a version, one line each —
# firmware checks against the baseline, A/B renders against a revision (default
# HEAD; settings from XT_* env), the plugin build, PluginTest (TEST_* env),
# the engine tests and auval.
source "$(dirname "$0")/env.sh"
D="$(dirname "$0")"; rc=0
"$D/fcheck.sh" || rc=1
"$D/ab.sh" "${1:-HEAD}" || rc=1
(cd "$REPO" && SDKROOT="$SDK" cmake --build build/plugin -j 10 > "$WORK/build_plugin.log" 2>&1 \
  && SDKROOT="$SDK" cmake --build build/plugin --target VSynthEmuTest -j 10 >> "$WORK/build_plugin.log" 2>&1) \
  && echo "plugin: built" || { echo "plugin: BUILD FAILED (see $WORK/build_plugin.log)"; rc=1; }
"$D/pt.sh" || rc=1
echo "engine tests: $("$WORK/fcheck/bin/engine_tests" 2>&1 | tail -1)"
echo "auval: $(auval -v aumu VsEm Ystu 2>&1 | grep -o "AU VALIDATION SUCCEEDED\|FAIL.*" | head -1)"
exit $rc
