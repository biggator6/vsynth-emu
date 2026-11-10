// The harnesses' research switches, from the environment (plugin/Source/XtSwitches.h).  build.sh and ab.sh link it into
// their programs with XT_HARNESS_SWITCHES defined; the plugin's targets never have it, so they read none.
#include "../../plugin/Source/XtSwitches.h"
#include <cstdlib>

const char* xtSwitchFromEnvironment(const char* name) { return std::getenv(name); }
