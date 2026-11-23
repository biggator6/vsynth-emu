#pragma once
// The instrument the plugin is (v453; research/UI_DESIGN_VC2.md, d-15, the user's answer of 2026-11-22): V-SYNTH or VC-2,
// a session setting, the processor's, kept in its state as the keyboard's fold is and saved with it, not a host
// parameter, so a host can't switch a whole engine mid-note and the parameters' list is as it was.  The header's switch
// sets it; it chooses which pages the editor shows, and (once step 4.3's Vc2Synth is whole) which engine plays.  A
// session from before v453 has none and opens on the V-Synth.
#include <juce_data_structures/juce_data_structures.h>

enum class PluginMode { vsynth = 0, vc2 = 1 };
inline const juce::Identifier kPluginModeProperty { "instrumentMode" };

inline PluginMode pluginMode(const juce::ValueTree& state) {
    return (int)state.getProperty(kPluginModeProperty, (int)PluginMode::vsynth) == (int)PluginMode::vc2
               ? PluginMode::vc2
               : PluginMode::vsynth;
}

inline void setPluginMode(juce::ValueTree& state, PluginMode mode) {
    state.setProperty(kPluginModeProperty, (int)mode, nullptr);
}
