// Start Offset's text in time, both ways (PluginStartOffset.h; the user's d-7, v478)
#include "PluginStartOffset.h"
#include "PluginParamIds.h"
#include <cmath>

namespace {
constexpr double kSamplesPerMs = kStartOffsetRate / 1000.0;
constexpr int kMsPlaces = 2, kSecondPlaces = 3;
}

juce::String startOffsetText(int samples) {
    const int v = std::clamp(samples, 0, kStartOffsetMax);
    if (v < kStartOffsetRate) return juce::String(v / kSamplesPerMs, kMsPlaces) + " ms";
    return juce::String(v / (double)kStartOffsetRate, kSecondPlaces) + " s";
}

int startOffsetFromText(const juce::String& text) {
    const juce::String t = text.trim().toLowerCase();
    const juce::String unit = t.retainCharacters("abcdefghijklmnopqrstuvwxyz");
    const double number = t.getDoubleValue();
    const double samples = unit.startsWith("smp") || unit.startsWith("sample") ? number
                         : unit.startsWith("ms") || unit.isEmpty()          ? number * kSamplesPerMs
                         : unit.startsWith("s")                              ? number * kStartOffsetRate
                                                                             : number * kSamplesPerMs;
    if (!(samples > 0.0)) return 0;
    return samples >= (double)kStartOffsetMax ? kStartOffsetMax : (int)std::lround(samples);
}

juce::AudioParameterIntAttributes startOffsetAttributes() {
    return juce::AudioParameterIntAttributes()
        .withStringFromValueFunction([](int v, int) { return startOffsetText(v); })
        .withValueFromStringFunction([](const juce::String& t) { return startOffsetFromText(t); });
}
