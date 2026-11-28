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
    const auto& edits = midi_.edits;   // a MIDI edit's value first (v462)
    return std::lround(edits.read(reads_.oscType)) != Params::kOscTypePcm || edits.read(reads_.osc2Switch) >= 0.5f;
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
        toSynth(meta.getMessage());   // the one chain the editor's messages take too (PluginProcess.cpp, v201)
    }
    if (pos < numSamples) renderSpan(buffer, inputs, L, R, pos, numSamples);
}
