#pragma once
// A VC-2 patch on the processor (v452): the .p02 loaded last, or a session's, its store and name kept; its values onto the
// VC-2's parameters (VSynthEmuProcessor::loadVc2Patch, PluginVc2Patch.cpp) and the patch in a session's state, beside
// the parameters, for what they don't hold (zones 3-16, the arpeggio pattern).  HARDWARE_FIRMWARE.md, "A .p02 onto the
// VC-2's parameters".
#include "PluginVc2Params.h"
#include "XtArpeggio.h"
#include <mutex>
#include <optional>
#include <vector>

constexpr int kVc2FirstStored = 0x120;   // [FW] the store's first ID, Patch Name 1; the system's are below it

// What the processor keeps of it: the message thread's, the lock for a host's state calls from another thread
struct PluginVc2Patch {
    mutable std::mutex lock;
    std::vector<uint8_t> store;   // kVc2StoreBytes, as rom.4 holds a patch; empty before one
    juce::String name;
    // v455: moved by each load and each session's restore, so the part's patch (PluginVc2Play) starts again from the
    // store; the store meanwhile what the part plays, the parameters written over the one loaded as they move
    long gen = 0;
};
// The value a store gives a VC-2 parameter, its ID read in its zone and COSM as rom.4 reads it; none for the system's,
// which no patch holds
std::optional<int> vc2PatchValue(const std::vector<uint8_t>& store, const Vc2HostParam& p);
// v455: the other way: a VC-2 parameter's value written where rom.4 reads it (a typed ID in its block's slot, a Start
// Offset whole); nothing for the system's
void vc2PatchPut(std::vector<uint8_t>& store, const Vc2HostParam& p, int value);
// v455: a store holding every ID's default (less nothing: the descriptor's raw default) in each zone, COSM and line its
// sections hold, for the parameters to be written over when no .p02 has come
std::vector<uint8_t> vc2DefaultStore();
// The arpeggio pattern a store holds, as the PLAY page draws it (v453): its 16 lines by rom.4's IDs from 0x4b4, read by
// line as rom.2's are (xtArpPatternRead); the XT's own pattern, every cell REST, with no store
XtArpPattern vc2ArpPattern(const std::vector<uint8_t>& store);
// The patch into a session's state, and back out of one: a session without one (from before v452) leaves none
void vc2PatchToState(const PluginVc2Patch& patch, juce::ValueTree& state);
void vc2PatchFromState(PluginVc2Patch& patch, const juce::ValueTree& state);
