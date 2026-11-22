#pragma once
// A VC-2 patch on the processor (v452): the .p02 loaded last, or a session's, its store and name kept; its values onto the
// VC-2's parameters (VSynthEmuProcessor::loadVc2Patch, PluginVc2Patch.cpp) and the patch in a session's state, beside
// the parameters, for what they don't hold (zones 3-16, the arpeggio pattern).  HARDWARE_FIRMWARE.md, "A .p02 onto the
// VC-2's parameters".
#include "PluginVc2Params.h"
#include <mutex>
#include <optional>
#include <vector>

// What the processor keeps of it: the message thread's, the lock for a host's state calls from another thread
struct PluginVc2Patch {
    mutable std::mutex lock;
    std::vector<uint8_t> store;   // kVc2StoreBytes, as rom.4 holds a patch; empty before one
    juce::String name;
};
// The value a store gives a VC-2 parameter, its ID read in its zone and COSM as rom.4 reads it; none for the system's,
// which no patch holds
std::optional<int> vc2PatchValue(const std::vector<uint8_t>& store, const Vc2HostParam& p);
// The patch into a session's state, and back out of one: a session without one (from before v452) leaves none
void vc2PatchToState(const PluginVc2Patch& patch, juce::ValueTree& state);
void vc2PatchFromState(PluginVc2Patch& patch, const juce::ValueTree& state);
