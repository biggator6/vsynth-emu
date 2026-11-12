// A setting's host parameter and its texts (PluginSetting.h): moved from PluginArp.cpp (v190) when the Multi Step
// Modulator's settings came to share them (v193).
#include "PluginSetting.h"

namespace Setting {

juce::String plainText(int v) { return juce::String(v); }
int plainValue(const juce::String& t) { return t.getIntValue(); }
juce::String signedText(int v) { return v >= 0 ? "+" + juce::String(v) : juce::String(v); }
juce::String stepText(int v) { return juce::String(v + 1); }
int stepValue(const juce::String& t) { return t.getIntValue() - 1; }

std::unique_ptr<juce::RangedAudioParameter> paramOf(const Spec& s) {
    switch (s.kind) {
        case Kind::Toggle: return std::make_unique<juce::AudioParameterBool>(s.id, s.name, s.def != 0);
        case Kind::Choice: return std::make_unique<juce::AudioParameterChoice>(s.id, s.name, s.values, s.def);
        case Kind::Number: break;
    }
    const Text t = s.text;
    const auto text = juce::AudioParameterIntAttributes()
                          .withStringFromValueFunction([t](int v, int) { return t.show(v); })
                          .withValueFromStringFunction([t](const juce::String& typed) { return t.read(typed); });
    return std::make_unique<juce::AudioParameterInt>(s.id, s.name, s.lo, s.hi, s.def, text);
}

} // namespace Setting
