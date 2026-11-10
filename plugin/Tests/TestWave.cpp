// The tests' wave (TestWave.h), shared by TeardownTest and RestoreTest.
#include "TestWave.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <cmath>
#include <memory>
#include <numbers>

juce::File writeTestWave(const juce::File& folder, int seconds) {
    constexpr int kRate = 44100;
    const int frames = seconds * kRate;
    juce::AudioBuffer<float> audio(1, frames);
    uint32_t noise = 1;
    for (int i = 0; i < frames; ++i) {
        const double t = (double)i / kRate, f = 110.0 * std::pow(2.0, (double)((i / (kRate / 2)) % 12) / 12.0);
        const double env = 0.6 + 0.4 * std::exp(-8.0 * std::fmod(t, 0.5));
        noise = noise * 1664525u + 1013904223u;
        const double n = (double)(noise >> 8) / 16777216.0 - 0.5;
        audio.setSample(0, i, (float)(0.4 * std::sin(2.0 * std::numbers::pi * f * t) * env + 0.01 * n));
    }
    const juce::File file = folder.getChildFile("wave_" + juce::String(seconds) + "s.wav");
    file.deleteFile();
    juce::WavAudioFormat wav;
    auto stream = std::make_unique<juce::FileOutputStream>(file);
    if (!stream->openedOk()) return {};
    std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.get(), kRate, 1, 16, {}, 0));
    if (!writer) return {};
    stream.release();   // the writer owns it now
    return writer->writeFromAudioSampleBuffer(audio, 0, frames) ? file : juce::File();
}
