# Sourced by the tools: paths and toolchain.  Override any of these in the environment.
REPO="${REPO:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)}"
ROM="${ROM:-$REPO/firmware/unpacked/V-SynthXT.rom.2.bin}"   # the user's rom.2 (gitignored; never commit)
WORK="${WORK:-$REPO/build/tools}"                           # binaries, inputs, results (gitignored)
SDK="${SDK:-/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk}"
TC="${TC:-/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin}"
PY="${PY:-$REPO/.venv/bin/python3}"                         # the system python lacks numpy
export JAVA_HOME="${JAVA_HOME:-/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home}"
mkdir -p "$WORK"
