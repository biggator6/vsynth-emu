#pragma once
// MIDI into a processor for a test (v463, moved from MidiTest.cpp, v462's, when the editor's learn checks came): a
// controller on channel 1, and one block with messages at its first frame.  And (v465) what the synth itself takes of
// a controller, by a Matrix route from it.
#include "TestControls.h"
#include "../Source/PluginProcessor.h"
#include <initializer_list>
#include <utility>
#include <vector>

inline juce::MidiMessage controller(int number, int value) {
    return juce::MidiMessage::controllerEvent(1, number, value);
}

inline void playMessages(VSynthEmuProcessor& p, std::initializer_list<juce::MidiMessage> messages, int block = 480) {
    juce::AudioBuffer<float> buffer(2, block);
    buffer.clear();
    juce::MidiBuffer midi;
    for (const auto& m : messages) midi.addEvent(m, 0);
    p.processBlock(buffer, midi);
}

// One block with messages at its first frame, then `more` with none
inline void holdBlocks(VSynthEmuProcessor& p, std::initializer_list<juce::MidiMessage> first, int more) {
    playMessages(p, first);
    for (int b = 0; b < more; ++b) playMessages(p, {});
}

// What the synth itself takes of a controller, read by its Matrix sum (XtSynthView; v465, v464's ring check's route
// made one): Matrix Control 1 from controller cc (1..31, the source's value its number) onto OSC 1's Level at +63, the
// Level stored at 64, OSC 1 on ANALOG so a note plays with no wave.  The IDs it set, for toDefaults after.
inline std::vector<juce::String> levelRoute(VSynthEmuProcessor& p, int cc) {
    const std::pair<juce::String, float> route[] = { { VSynthEmuProcessor::matrixParamId(0, "Source"), (float)cc },
                                                     { VSynthEmuProcessor::matrixParamId(0, "Dest1"), 4 },
                                                     { VSynthEmuProcessor::matrixParamId(0, "Sens1"), 63 },
                                                     { "oscLevel", 64 }, { "oscType", 0 } };
    std::vector<juce::String> ids;
    for (const auto& [id, v] : route) {
        setParam(p, id, v);
        ids.push_back(id);
    }
    return ids;
}

inline void toDefaults(VSynthEmuProcessor& p, const std::vector<juce::String>& ids) {
    for (const auto& id : ids) {
        auto* param = p.apvts.getParameter(id);
        param->setValueNotifyingHost(param->getDefaultValue());
    }
}
