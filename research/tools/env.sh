# Sourced by the tools: paths and toolchain.  Override any of these in the environment.
# The checkout is two folders above this file, which bash names in BASH_SOURCE and zsh, which has none, in $0 while it
# sources it (FUNCTION_ARGZERO, its default).  A REPO that doesn't hold this file is refused, not used: from zsh, REPO
# used to land two folders above the current one, with build/tools made there (v178, env_test.sh).
REPO="${REPO:-$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")/../.." && pwd)}"
if [ ! -f "$REPO/research/tools/env.sh" ]; then
    echo "env.sh: REPO is $REPO, not a checkout; source env.sh from bash or zsh by its path, or set REPO" >&2
    return 1 2> /dev/null || exit 1
fi
ROM="${ROM:-$REPO/firmware/unpacked/V-SynthXT.rom.2.bin}"   # the user's rom.2 (gitignored; never commit)
WORK="${WORK:-$REPO/build/tools}"                           # binaries, inputs, results (gitignored)
SDK="${SDK:-/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk}"
TC="${TC:-/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin}"
PY="${PY:-$REPO/.venv/bin/python3}"                         # the system python lacks numpy
# What a harness is built with to read the research switches (plugin/Source/XtSwitches.h); the plugin's targets never are
HARNESS_SWITCHES=(-DXT_HARNESS_SWITCHES "$REPO/research/tools/harness_switches.cpp")
export JAVA_HOME="${JAVA_HOME:-/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home}"
mkdir -p "$WORK"
