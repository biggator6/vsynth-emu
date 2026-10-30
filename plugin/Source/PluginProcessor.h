#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "VariphraseEngine.h"
#include "XtSynth.h"

class VSynthEmuProcessor : public juce::AudioProcessor,
                           public juce::AudioProcessorValueTreeState::Listener,
                           private juce::AsyncUpdater
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

    // ── Algorithm selection (the engine path used without the firmware) ─────
    void setAlgorithm(VSE::Algorithm algo);
    VSE::Algorithm getAlgorithm() const;

    // ── File loading ─────────────────────────────────────────────────────────
    // Message thread.  Reads the WAV/AIFF and encodes it in the background
    // as the selected Encode Type (the XT path needs the firmware image).
    void loadFile(const juce::File& file);

    juce::String getLoadedFileName() const  { return loadedFileName_; }
    bool         hasFileLoaded()    const   { return fileLoaded_.load(); }

    // ── The XT's firmware (rom.2) ────────────────────────────────────────────
    // Message thread.  Returns false if the file is not a usable image.
    bool setRomFile(const juce::File& file);
    bool hasRom() const { return xtSynthRomReady(); }
    // One line for the editor: ROM, encode progress, current encode type.
    juce::String getStatusText() const;

    juce::AudioFormatManager& getFormatManager() { return formatManager_; }

private:
    VSE::VariphraseEngine engine_;
    XtSynth synth_;
    std::atomic<bool> xtActive_ { false };      // the XT voices own the output
    std::vector<float> scratch_;                // right channel for a mono output bus

    juce::AudioFormatManager formatManager_;

    // ── File buffer ──────────────────────────────────────────────────────────
    // Written on the message thread, read on the audio thread (legacy path).
    juce::AudioBuffer<float> fileBuffer_;
    juce::CriticalSection    bufferLock_;
    std::atomic<bool>        fileLoaded_ { false };
    juce::String             loadedFileName_;
    double                   fileRate_ = 44100.0;
    int                      fileBits_ = 16;

    // ── Encoding (background) ────────────────────────────────────────────────
    juce::ThreadPool encodePool_ { 1 };
    std::atomic<int> encodeGen_ { 0 };
    mutable juce::CriticalSection statusLock_;
    juce::String status_;
    void reencode();
    void setStatus(const juce::String& s);
    void handleAsyncUpdate() override;          // re-encode after an Encode Type / Depth change
    void findRom();

    // ── Playback state (audio thread, legacy path) ───────────────────────────
    int  playbackPos_  = -1;
    int  activeVoices_ = 0;

    void syncParamsToEngine();
    XtSynth::Controls xtControls() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VSynthEmuProcessor)
};
