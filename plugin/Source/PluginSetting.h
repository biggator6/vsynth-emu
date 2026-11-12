#pragma once
// One of the XT's settings as a host parameter: a Toggle (0…1), a Choice of the firmware's values, or a Number with its
// text as the XT shows it.  The arpeggio's ten (PluginArp.cpp, v190) and the Multi Step Modulator's 98 (PluginMsm.cpp,
// v193) are made by it; moved from PluginArp.cpp when the MSM's came (v193).
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>

namespace Setting {

enum class Kind { Toggle, Choice, Number };

// A Number's text and its reading back
struct Text {
    juce::String (*show)(int);
    int (*read)(const juce::String&);
};
juce::String plainText(int v);
int plainValue(const juce::String& t);
juce::String signedText(int v);           // "+0", as the XT
// A count from 1 where the firmware's routine gives 0…: the arpeggio's End Point (1…32), the MSM's End Point and Zone
// (1…16), as the manual has them
juce::String stepText(int v);
int stepValue(const juce::String& t);
inline constexpr Text kPlain { plainText, plainValue };

struct Spec {
    juce::String id;          // the plugin's, never changed
    juce::String name;
    Kind kind;
    int lo, hi, def;
    juce::StringArray values; // a Choice's
    Text text;                // a Number's
};
std::unique_ptr<juce::RangedAudioParameter> paramOf(const Spec& s);

} // namespace Setting
