// The VC-2's PATCH Init by Patch Category on the processor: see PluginVc2PatchInit.h (v473).
#include "PluginVc2PatchInit.h"
#include "PluginProcessor.h"
#include "Vc2PatchInit.h"

bool initVc2Patch(VSynthEmuProcessor& p, int category, std::string* why) {
    Vc2PatchFile init;
    if (!vc2PatchInit(category, init, why)) return false;
    p.applyVc2Patch(init.name, init.store);
    return true;
}
