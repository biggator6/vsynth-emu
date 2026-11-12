// The parameters' layout: the note values', two places' and velocity curves' texts, then the parts in the order a host
// has always seen them; and OSC k's parameter IDs.  Moved as they stood from PluginProcessor.cpp (v190).
#include "PluginProcessor.h"
#include "PluginParamIds.h"

juce::String VSynthEmuProcessor::oscParamId(int k, const juce::String& id) { return Params::oscId(k, id); }

juce::AudioProcessorValueTreeState::ParameterLayout
VSynthEmuProcessor::createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // An envelope's time or an LFO's rate from 128 on is a note value, as the XT's knobs go on past 127: 3 to 192 clocks of 24 to the quarter
    static const char* const kNotes[17] = { "1/32", "1/16", "1/8T", "1/16.", "1/8", "1/4T", "1/8.", "1/4", "1/2T", "1/4.", "1/2", "1/1T", "1/2.", "1/1", "2/1T", "1/1.", "2/1" };
    const auto noteValue = juce::AudioParameterIntAttributes()
        .withStringFromValueFunction([](int v, int) { return v >= 128 ? juce::String(kNotes[std::min(v - 128, 16)]) : juce::String(v); })
        .withValueFromStringFunction([](const juce::String& t) {
            for (int i = 0; i < 17; ++i) if (t.trim() == kNotes[i]) return 128 + i;
            return t.getIntValue();
        });
    // A wave's own loop and tempo hold more than a box can show
    const auto twoPlaces = juce::AudioParameterFloatAttributes().withStringFromValueFunction([](float v, int) { return juce::String(v, 2); });
    auto curveName = juce::AudioParameterIntAttributes().withStringFromValueFunction(
        [](int v, int) { return v == 0 ? juce::String("OFF") : juce::String(v); });
    // The parts, in the order a host has always seen them (PluginParams*.cpp, v190)
    addOscParams(params, twoPlaces);
    addCosmParams(params, noteValue);
    addToneParams(params, noteValue, curveName);
    addOsc2Params(params, noteValue, twoPlaces, curveName);
    addModParams(params);
    addArpParams(params);   // PluginArp.cpp, v190: appended, so every other keeps its place
    addMsmParams(params);   // PluginMsm.cpp, v193: the same

    return { params.begin(), params.end() };
}
