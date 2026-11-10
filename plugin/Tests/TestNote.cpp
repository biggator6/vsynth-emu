#include "TestNote.h"

std::vector<float> playNote(VSynthEmuProcessor& p, int block, int blocks, int channels) {
    p.prepareToPlay(48000.0, block);
    juce::AudioBuffer<float> buf(2, block);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100), 0);
    std::vector<float> out;
    for (int b = 0; b < blocks; ++b) {
        buf.clear();
        p.processBlock(buf, midi);
        midi.clear();
        for (int ch = 0; ch < channels; ++ch)
            out.insert(out.end(), buf.getReadPointer(ch), buf.getReadPointer(ch) + block);
    }
    return out;
}
