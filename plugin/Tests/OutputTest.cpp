// The plugin's output always numbers (v454): a float wave holding NaN and ±infinity, played with no image (the file as
// it is), comes out with every sample a finite number and the wave's own samples still heard.  A NaN handed to a host
// spreads through its mix and stays in its effects' feedback until they're reset; step 4.3's v349 found a way for the
// VC-2's voices to make one (VC2_MODE_PLAN.md, step 4.5's "A NaN sample to the host").
// Run: VSynthEmuOutputTest <rom.2.bin> (not read: the processor runs with no image).  Exit 0 if every check holds.
#include "TestChecks.h"
#include "TestMain.h"
#include "TestWave.h"
#include "../Source/PluginProcessor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <string>

namespace {

constexpr int kRate = 44100, kBlock = 512, kBlocks = 40;
constexpr int kOddEvery = 100;           // frames between the wave's samples that aren't numbers
constexpr float kSineLevel = 0.5f;
constexpr double kHeardAtLeast = 0.03;   // the sine at ⅛, the level the file plays at with no image (0.0625), heard

// A second of a float wave, mono: a sine at half scale with NaN, +infinity and −infinity in turn every 100 frames
juce::AudioBuffer<float> withNonNumbers() {
    const float odd[] = { std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
                          -std::numeric_limits<float>::infinity() };
    juce::AudioBuffer<float> audio(1, kRate);
    for (int i = 0; i < kRate; ++i) {
        const float sine = kSineLevel * std::sin(0.0628f * (float)i);
        audio.setSample(0, i, i % kOddEvery == 0 ? odd[(i / kOddEvery) % 3] : sine);
    }
    return audio;
}

// How many of the file's samples, as a reader gives them, aren't finite: the run can fail only if some aren't
int nonFiniteRead(const juce::File& file) {
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    const std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if (!reader) return 0;
    juce::AudioBuffer<float> read(1, (int)reader->lengthInSamples);
    reader->read(&read, 0, read.getNumSamples(), 0, true, false);
    const float* x = read.getReadPointer(0);
    return (int)std::count_if(x, x + read.getNumSamples(), [](float s) { return !std::isfinite(s); });
}

int run(const juce::File&, const juce::File& work) {
    TestChecks check { "output" };
    const juce::File file = work.getChildFile("nonnumbers.wav");
    check(writeWav(file, withNonNumbers(), kRate, 32), "the float wave should be written");
    const int odd = nonFiniteRead(file);
    check(odd > 0, "the wave as read should hold samples that aren't numbers");
    VSynthEmuProcessor p;
    check(!p.hasRom(), "the processor should run with no image");
    p.prepareToPlay(kRate, kBlock);
    p.loadFile(file);
    juce::AudioBuffer<float> buf(2, kBlock);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100), 0);
    int notNumbers = 0;
    double peak = 0;
    for (int b = 0; b < kBlocks; ++b) {
        p.processBlock(buf, midi);
        midi.clear();
        for (int ch = 0; ch < buf.getNumChannels(); ++ch) {
            const float* x = buf.getReadPointer(ch);
            for (int i = 0; i < kBlock; ++i) {
                if (!std::isfinite(x[i])) ++notNumbers;
                else peak = std::max(peak, (double)std::fabs(x[i]));
            }
        }
    }
    check(notNumbers == 0, std::to_string(notNumbers) + " samples out weren't finite numbers");
    check(peak > kHeardAtLeast, "the wave's own samples should still come out (peak " + std::to_string(peak) + ")");
    return check.summary(std::to_string(odd) + " of the wave's samples not numbers");
}

} // namespace

int main(int argc, char** argv) { return testMain(argc, argv, "output", run); }
