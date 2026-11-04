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
    // The Matrix's parameters: matrixNSource, matrixNDest1, … for N = 1…8; the system's controller assigns
    static juce::String matrixParamId(int control, const char* name) { return "matrix" + juce::String(control + 1) + name; }
    static const char* assignParamId(int which);
    static const char* assignName(int which);
    static constexpr int kAssigns = 12;
    static int assignDefault(int which);            // its value: the pad's two pairs on CC80…CC83, the others OFF
    static juce::StringArray matrixSourceNames();   // the XT's Source values, 0…108
    static juce::StringArray matrixDestNames();     // and its Dest values, 0…78
    static juce::StringArray assignValueNames();    // OFF, CC01…CC95
    static juce::String oscParamId(int k, const juce::String& id);   // OSC k's parameter (OSC 2: "osc2" before it)
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
    // as the selected Encode Type (the XT path needs the firmware image).  Each
    // OSC has its own wave (osc: 0 OSC 1, 1 OSC 2), as on the XT; the engine path
    // without the firmware plays OSC 1's.
    void loadFile(const juce::File& file, int osc = 0);

    juce::String getLoadedFileName(int osc = 0) const  { return wave_[osc].name; }
    bool         hasFileLoaded(int osc = 0)    const   { return wave_[osc].loaded.load(); }

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
    int tripKnob_ = 0;                          // the Time Trip knob as XtSynth last had it

    juce::AudioFormatManager formatManager_;

    // ── File buffers, one per OSC ────────────────────────────────────────────
    // Written on the message thread, OSC 1's read on the audio thread (legacy path).
    struct LoadedWave {
        juce::AudioBuffer<float> buffer;
        std::atomic<bool>        loaded { false };
        juce::String             name;
        double                   rate = 44100.0;
        int                      bits = 16;
        std::atomic<int>         gen { 0 };          // a newer encode request discards an older result
        std::atomic<bool>        reencode { false }; // its Encode Type or Depth changed
    };
    LoadedWave               wave_[2];
    juce::CriticalSection    bufferLock_;

    // ── Encoding (background) ────────────────────────────────────────────────
    juce::ThreadPool encodePool_ { 1 };
    mutable juce::CriticalSection statusLock_;
    juce::String status_;
    void reencode(int osc);
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
