#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "VariphraseEngine.h"

class VSynthEmuProcessor : public juce::AudioProcessor,
                           public juce::AudioProcessorValueTreeState::Listener
{
public:
    VSynthEmuProcessor();
    ~VSynthEmuProcessor() override;

    // ── AudioProcessor overrides ────────────────────────────────────────────
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "V-Synth Emu"; }

    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }

    double getTailLengthSeconds() const override { return 0.5; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // ── Parameter state ─────────────────────────────────────────────────────
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void parameterChanged(const juce::String& paramID, float newValue) override;

    // ── Algorithm selection ──────────────────────────────────────────────────
    void setAlgorithm(VSE::Algorithm algo);
    VSE::Algorithm getAlgorithm() const;

    // ── File loading ─────────────────────────────────────────────────────────
    // Call from the message thread only. Reads the WAV/AIFF into memory and
    // resamples to the current playback sample rate if necessary.
    void loadFile(const juce::File& file);

    juce::String getLoadedFileName() const  { return loadedFileName_; }
    bool         hasFileLoaded()    const   { return fileLoaded_.load(); }

    // Exposed so the editor can build the file-chooser wildcard string
    juce::AudioFormatManager& getFormatManager() { return formatManager_; }

private:
    VSE::VariphraseEngine engine_;

    // ── Format manager ───────────────────────────────────────────────────────
    juce::AudioFormatManager formatManager_;

    // ── File buffer ──────────────────────────────────────────────────────────
    // Written on the message thread, read on the audio thread.
    // Protected by bufferLock_; audio thread uses tryEnter so it never blocks.
    juce::AudioBuffer<float> fileBuffer_;
    juce::CriticalSection    bufferLock_;
    std::atomic<bool>        fileLoaded_ { false };
    juce::String             loadedFileName_;

    // ── Playback state (audio thread) ────────────────────────────────────────
    int  playbackPos_  = -1;   // sample position into fileBuffer_; -1 = stopped
    int  activeVoices_ = 0;    // count of currently held notes

    void syncParamsToEngine();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VSynthEmuProcessor)
};
