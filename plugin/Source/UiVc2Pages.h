#pragma once
// The VC-2 mode's pages under the page bar but SYNTH (v453; research/UI_DESIGN_VC2.md, "Each page's cards"): VOCODER,
// MODULATION, FX, PLAY and ZONES, in Blocks::kVc2Pages' order, each D's page or its cards made from the VC-2's tables,
// its rows rom.4's IDs made the host's by vc2HostParamId.
#include "PluginProcessor.h"
#include <memory>

namespace Vc2Pages {

// The page `page` of Blocks::kVc2Pages, made for a processor; none for SYNTH (Vc2SynthPage)
std::unique_ptr<juce::Component> makeVc2Page(int page, VSynthEmuProcessor& p);
// A patch row's host ID: rom.4's hex ID of the patch's (or the system's) parameter, "4a9" vc2_4a9
juce::String patchId(const char* row);

}
