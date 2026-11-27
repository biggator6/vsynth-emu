// The V-Synth's PATCH Init on the processor: see PluginPatchInit.h (v473).
#include "PluginPatchInit.h"
#include "PluginProcessor.h"
#include "XtFwPatchInit.h"

bool initPatch(VSynthEmuProcessor& p, std::string* why) {
    if (!xtSynthRomReady()) return xtfile::fail(why, "A patch init needs the firmware image");
    XtPatchFile init;
    if (!xtFwPatchInit(init, why)) return false;
    p.applyPatch(init.name, init.store, 0, juce::File());   // no project: the init names no wave
    return true;
}
