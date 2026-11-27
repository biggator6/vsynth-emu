#pragma once
// MIDI into a processor for a test (v463, moved from MidiTest.cpp, v462's, when the editor's learn checks came): a
// controller on channel 1, and one block with messages at its first frame.
#include "../Source/PluginProcessor.h"
#include <initializer_list>

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
