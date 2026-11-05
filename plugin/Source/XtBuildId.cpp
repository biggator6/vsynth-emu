#include "XtBuildId.h"
#include "XtBuildIdGenerated.h"

// The whole line stays in the binary, so an installed copy can be read without loading it: research/tools/buildid.sh.
// `used`, as link-time optimisation can otherwise keep only the part after the mark, and did, on the AU and VST3 of some ids and not others.
static constexpr char kMark[] = "V-Synth Emu build ";
[[gnu::used]] static const char kLine[] = "V-Synth Emu build " XT_BUILD_ID;

const char* xtBuildId() { return kLine + sizeof(kMark) - 1; }
const char* xtBuildTime() { return XT_BUILD_TIME; }
