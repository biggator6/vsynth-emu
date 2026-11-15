// The XT's players in processBlock, moved from it as they stood (v207): whether an OSC plays with no wave, and the MIDI
// at its frames with the render between, now through renderSpan, which takes the outside input first (PluginExtIn.cpp).
#include "PluginProcessor.h"
#include "PluginParamIds.h"

VSynthEmuProcessor::BlockReads::BlockReads(juce::AudioProcessorValueTreeState& s)
    : oscType(s.getRawParameterValue(Params::kOscType)),
      osc2Switch(s.getRawParameterValue(Params::oscId(1, Params::kOscSwitch))),
      extInType(s.getRawParameterValue("fxExtInType")) {}

// An analog or EXT IN OSC needs the firmware image but no sample: OSC 1's Type anything but PCM, or OSC 2 on
bool VSynthEmuProcessor::playsWaveless() const {
    if (!xtSynthRomReady()) return false;
    return std::lround(reads_.oscType->load()) != Params::kOscTypePcm || reads_.osc2Switch->load() >= 0.5f;
}

void VSynthEmuProcessor::playMidi(const juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi, int inputs,
                                  float* L, float* R) {
    const int numSamples = buffer.getNumSamples();
    int pos = 0;
    for (const auto meta : midi) {
        const int at = std::clamp(meta.samplePosition, pos, numSamples);
        if (at > pos) {
            renderSpan(buffer, inputs, L, R, pos, at);
            pos = at;
        }
        const auto msg = meta.getMessage();
        if (msg.isNoteOn()) synth_.noteOn(msg.getNoteNumber(), msg.getFloatVelocity());
        else if (msg.isNoteOff()) synth_.noteOff(msg.getNoteNumber(), msg.getFloatVelocity());
        else if (msg.isPitchWheel()) synth_.pitchBend(msg.getPitchWheelValue());
        else if (msg.isChannelPressure()) synth_.channelPressure(msg.getChannelPressureValue());
        else if (msg.isAftertouch()) synth_.polyPressure(msg.getNoteNumber(), msg.getAfterTouchValue());
        else if (msg.isController()) synth_.controller(msg.getControllerNumber(), msg.getControllerValue());   // the mode messages too
    }
    if (pos < numSamples) renderSpan(buffer, inputs, L, R, pos, numSamples);
}
