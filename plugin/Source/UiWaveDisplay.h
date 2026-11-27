#pragma once
// What an OSC panel's display draws (v200; UiOscPanel): a PCM OSC's loaded wave, from the processor's view of it, its
// loop's span shaded; an analog OSC's waveform, a few cycles of the plugin's own analog oscillator (XtAnalogOsc) at the
// panel's settings, so each waveform, its Pulse Width, Impact and the rest are drawn as the OSC plays them.
#include "PluginProcessor.h"
#include "XtAnalog.h"
#include <vector>

namespace WaveDisplay {

using View = VSynthEmuProcessor::WaveView;

// What an analog OSC's picture is made from: its oscillator's settings and Wave Gain, as the controls hold them, and
// whether it is OSC 2, where X-MOD has no depth (xtAnalogBlock)
struct AnalogSettings {
    XtAnalogParams ana;
    int waveGain = 0;
    bool secondOsc = false;
    bool operator==(const AnalogSettings&) const = default;
};
AnalogSettings analogSettings(const XtSynth::Controls& c, int osc);
// Three cycles of the analog oscillator at those settings, after its start has settled, scaled to ±1 by their peak;
// none without the firmware's tables
std::vector<float> analogCycles(const AnalogSettings& s);

// A wave's loop, as fractions of the wave, and whether it is on
struct Loop {
    bool on = false;
    float start = 0.0f, end = 1.0f;
    bool operator==(const Loop&) const = default;
};
void paintWave(juce::Graphics& g, juce::Rectangle<float> area, const View& view, const Loop& loop);
// The Start Offset's line across a wave's area at x, in the accent (v479: the rack's display while it's held, the WAVE
// page's always)
void paintStartLine(juce::Graphics& g, juce::Rectangle<float> area, float x);
void paintCycles(juce::Graphics& g, juce::Rectangle<float> area, const std::vector<float>& samples);

} // namespace WaveDisplay
