#include "TestNote.h"
#include "TestControls.h"
#include <chrono>
#include <thread>

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

juce::String encodeWave(VSynthEmuProcessor& p, const juce::File& wave, float type, int seconds) {
    constexpr int kPollMs = 50;
    setParam(p, "encodeType", type);
    p.loadFile(wave);
    const auto done = [&p] {
        const juce::String s = p.getStatusText();
        return s.startsWith("Encoded") || s.startsWith("Encode failed");
    };
    for (int i = 0; i < seconds * 1000 / kPollMs && !done(); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(kPollMs));
    return p.getStatusText();
}
