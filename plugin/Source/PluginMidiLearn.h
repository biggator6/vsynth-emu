#pragma once
// MIDI learn's map (v463; research/NEXT_ISSUES.md item 1, the user's d-3 and d-4): a controller number to a parameter,
// one controller a parameter.  A control arms it for its parameter (the editor's menu, UiMidiLearn.h); the next
// controller the host sends is then that parameter's.  A learnt controller's every value moves its parameter over its
// whole range, through PluginMidiEdits as the firmware's edits do, and is the plugin's alone: it doesn't reach the
// firmware, so one controller never does two things (d-3).  The map is the session's (d-4), by parameter ID.  The
// audio thread reads and learns; the message thread arms, forgets and keeps it; nothing locks.
#include "PluginMidiEdits.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <utility>
#include <vector>

class PluginMidiLearn {
public:
    static constexpr int kControllers = 128;
    explicit PluginMidiLearn(PluginMidiEdits& edits);

    // Message thread: the next controller learns the parameter at `index` (PluginMidiEdits' index), or none (-1)
    void arm(int index);
    int armed() const { return armed_.load(std::memory_order_acquire); }
    void forget(int index);                       // the parameter's controller, if it has one
    void forgetAll();
    int controllerOf(int index) const;            // -1 for none
    int parameterOf(int cc) const;                // -1 for none
    std::vector<std::pair<int, int>> entries() const;   // (controller, index), by controller
    uint32_t changes() const { return changes_.load(std::memory_order_acquire); }   // moves at each change
    juce::String toText() const;                  // the session's: "cc=parameter ID" by commas
    void fromText(const juce::String& text);      // a session's, the map before it gone

    // Audio thread: whether the map takes this controller (one being learnt, or one learnt)
    bool takes(int cc) const;
    // and takes it: one being learnt becomes the armed parameter's; a learnt one's value goes onto its parameter.  False
    // if it takes none
    bool take(int cc, int value);

private:
    void set(int cc, int index);   // the map's entry, the parameter's other controller let go
    PluginMidiEdits& edits_;
    std::array<std::atomic<int>, kControllers> map_;
    std::atomic<int> armed_ { -1 };
    std::atomic<uint32_t> changes_ { 0 };
};
