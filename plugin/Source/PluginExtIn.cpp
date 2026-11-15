// The outside input's two settings as host parameters (v206): External In Type and Source, the system's IDs 0x78 and
// 0x7c, with the firmware's names, ranges, defaults and values' text (its routine 0x89461a70, read against them in
// PluginTest's TEST_FX), appended after the zones' so every other keeps its place.  They reach the effects' driver
// through kFxKnobs (PluginEffects.cpp); HARDWARE_FIRMWARE.md, "The input's way in, the chip's side".
// And its audio (v207; "The input's audio"): the input bus, and the host's frames into XtHostInput before each render.
#include "PluginProcessor.h"
#include "PluginSetting.h"

namespace {
constexpr int kMostInputs = 2;   // the bus at its widest, stereo: L and R, the chip's two slots of each pair
}

// Off, mono or stereo in: the Standalone's device decides (JUCE's player tries its channels first), a host may turn the
// bus off.  The output as before v207, any.
bool VSynthEmuProcessor::isBusesLayoutSupported(const BusesLayout& layout) const {
    const juce::AudioChannelSet in = layout.getMainInputChannelSet();
    return in.isDisabled() || in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

bool VSynthEmuProcessor::hasInput() const { return getTotalNumInputChannels() > 0; }

int VSynthEmuProcessor::inputBlock(juce::AudioBuffer<float>& buffer) {
    const int inputs = std::min({ getTotalNumInputChannels(), kMostInputs, buffer.getNumChannels() });
    for (int ch = inputs; ch < buffer.getNumChannels(); ++ch) buffer.clear(ch, 0, buffer.getNumSamples());
    hostIn_.setType((int)std::lround(reads_.extInType->load()));
    return inputs;
}

// A span at most hostIn_.most() frames at a time, so what it holds never overruns; a mono input on both sides
void VSynthEmuProcessor::renderSpan(const juce::AudioBuffer<float>& in, int inputs, float* L, float* R, int from,
                                    int to) {
    for (int at = from; at < to;) {
        const int n = std::min(to - at, hostIn_.most());
        const float* l = inputs > 0 ? in.getReadPointer(0, at) : nullptr;
        const float* r = inputs > 1 ? in.getReadPointer(1, at) : l;
        hostIn_.push(l, r, n);
        synth_.render(L + at, R + at, n);
        at += n;
    }
}

void VSynthEmuProcessor::addExtInParams(ParamList& params) {
    using Setting::Kind;
    const Setting::Spec specs[] = {
        { "fxExtInType", "External In Type", Kind::Choice, 0, 3, 1, { "STEREO", "MONO-L+R", "MONO-L", "MONO-R" },
          Setting::kPlain },
        { "fxExtInSource", "External In Source", Kind::Choice, 0, 1, 0, { "Analog", "USB" }, Setting::kPlain },
    };
    for (const Setting::Spec& s : specs) params.push_back(Setting::paramOf(s));
}
