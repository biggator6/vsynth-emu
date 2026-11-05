#pragma once
// A .vpw as a JUCE audio format, so it loads wherever a WAV does: its audio unpacked by the firmware's own routine (XtFiles),
// and its encode type in the reader's metadata.  Needs the firmware image.
#include <juce_audio_formats/juce_audio_formats.h>
#include "XtFiles.h"

class XtVpwFormat : public juce::AudioFormat {
public:
    XtVpwFormat() : juce::AudioFormat("V-Synth Wave", ".vpw") {}
    juce::Array<int> getPossibleSampleRates() override { return { 44100 }; }
    juce::Array<int> getPossibleBitDepths() override { return { 16 }; }
    bool canDoStereo() override { return true; }
    bool canDoMono() override { return true; }

    juce::AudioFormatReader* createReaderFor(juce::InputStream* stream, bool deleteStreamIfOpeningFails) override {
        const auto start = stream->getPosition();
        juce::MemoryBlock bytes;
        stream->readIntoMemoryBlock(bytes);
        auto wave = std::make_unique<XtWaveFile>();
        if (!xtWaveRead(static_cast<const uint8_t*>(bytes.getData()), bytes.getSize(), *wave)) {
            if (deleteStreamIfOpeningFails) delete stream;
            else stream->setPosition(start);
            return nullptr;
        }
        return new Reader(stream, std::move(wave));
    }
    juce::AudioFormatWriter* createWriterFor(juce::OutputStream*, double, unsigned int, int, const juce::StringPairArray&, int) override { return nullptr; }
    using juce::AudioFormat::createWriterFor;

    static constexpr const char* kEncodeType = "vpsc encode type";

private:
    class Reader : public juce::AudioFormatReader {
    public:
        Reader(juce::InputStream* stream, std::unique_ptr<XtWaveFile> w) : juce::AudioFormatReader(stream, "V-Synth Wave"), wave(std::move(w)) {
            sampleRate = 44100.0;
            bitsPerSample = 16;
            lengthInSamples = wave->frames;
            numChannels = (unsigned int)wave->channels;
            usesFloatingPointData = false;
            metadataValues.set(kEncodeType, juce::String(wave->encodeType));
        }
        bool readSamples(int* const* dest, int numDest, int destOffset, juce::int64 start, int count) override {
            clearSamplesBeyondAvailableLength(dest, numDest, destOffset, start, count, lengthInSamples);
            if (count <= 0) return true;
            for (int c = 0; c < juce::jmin(numDest, wave->channels); ++c)
                if (int* d = dest[c])
                    for (int i = 0; i < count; ++i)
                        d[destOffset + i] = (int)wave->pcm[(size_t)((start + i) * wave->channels + c)] * 65536;
            return true;
        }
    private:
        std::unique_ptr<XtWaveFile> wave;
    };
};
