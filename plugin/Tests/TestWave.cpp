// The tests' waves (TestWave.h), shared by TeardownTest and RestoreTest, and the WAVE page's checks' V-Synth SOLO wave.
#include "TestWave.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <algorithm>
#include <cmath>
#include <cstring>
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
    return writeWav(file, audio, kRate, 16) ? file : juce::File();
}

namespace {

void put32(uint8_t* at, uint32_t v) {
    std::memcpy(at, &v, 4);
}

// A SOLO mono wave's vpdt with these events and nothing else: their count in u32[2]'s high half, each a word on from
// halfword 8 + 10 a event (XtWaveFile.cpp's layout)
juce::MemoryBlock soloVpdt(const std::vector<int32_t>& events) {
    constexpr size_t kStride = 10, kFirst = 8, kLeast = 64;
    const size_t n = events.size();
    juce::MemoryBlock vpdt(std::max(kLeast, n == 0 ? 0 : (kStride * (n - 1) + kFirst) * 2 + 8), true);
    auto* v = (uint8_t*)vpdt.getData();
    put32(v + 8, (uint32_t)n << 16);
    for (size_t e = 0; e < n; ++e) put32(v + (kStride * e + kFirst) * 2 + 4, (uint32_t)events[e]);
    return vpdt;
}

} // namespace

juce::File writeSoloWave(const juce::File& folder, const juce::String& name, long frames,
                         const std::vector<int32_t>& events) {
    juce::MemoryBlock fmt(16, true), vpsc(32, true), vplp(28, true), data((size_t)frames * 2, true);
    auto* f = (uint8_t*)fmt.getData();
    f[0] = 1;                                    // PCM
    f[2] = 1;                                    // mono
    put32(f + 4, 44100);
    put32(f + 8, 88200);
    f[12] = 2;
    f[14] = 16;
    auto* s = (uint8_t*)vpsc.getData();
    s[1] = 1;                                    // SOLO
    put32(s + 4, (uint32_t)frames);
    s[0xa] = 60;
    auto* l = (uint8_t*)vplp.getData();
    put32(l + 8, (uint32_t)(frames - 1));
    put32(l + 0xc, (uint32_t)(frames - 1));
    l[0x18] = 1;
    l[0x19] = 1;
    l[0x1a] = 1;
    auto* d = (int16_t*)data.getData();
    for (long i = 0; i < frames; ++i) d[i] = (int16_t)(12000.0 * std::sin(0.03 * (double)i));
    const juce::MemoryBlock vpdt = soloVpdt(events);
    std::vector<std::pair<const char*, const juce::MemoryBlock*>> chunks {
        { "fmt ", &fmt }, { "vpsc", &vpsc }, { "vplp", &vplp } };
    if (!events.empty()) chunks.push_back({ "vpdt", &vpdt });
    chunks.push_back({ "data", &data });
    juce::MemoryOutputStream body;
    body.write("WAVE", 4);
    for (const auto& [id, b] : chunks) {
        body.write(id, 4);
        body.writeInt((int)b->getSize());
        body.write(b->getData(), b->getSize());
    }
    const juce::File file = folder.getChildFile(name);
    file.deleteFile();
    juce::FileOutputStream out(file);
    out.write("RIFF", 4);
    out.writeInt((int)body.getDataSize());
    out.write(body.getData(), body.getDataSize());
    return file;
}

bool writeWav(const juce::File& file, const juce::AudioBuffer<float>& audio, int rate, int bits) {
    file.deleteFile();
    juce::WavAudioFormat wav;
    auto stream = std::make_unique<juce::FileOutputStream>(file);
    if (!stream->openedOk()) return false;
    std::unique_ptr<juce::AudioFormatWriter> writer(
        wav.createWriterFor(stream.get(), rate, (unsigned)audio.getNumChannels(), bits, {}, 0));
    if (!writer) return false;
    stream.release();   // the writer owns it now
    return writer->writeFromAudioSampleBuffer(audio, 0, audio.getNumSamples());
}
