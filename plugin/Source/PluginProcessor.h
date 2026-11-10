#pragma once

#include <map>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "VariphraseEngine.h"
#include "XtSynth.h"
#include "XtFiles.h"
#include "XtFxControl.h"
#include <climits>
#include <mutex>

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
    // A V-Synth wave (.vpw, or a project's .wav) brings its own Original Key, Tempo, loop and fine tuning to OSC's parameters unless waveSettings is false, as for a saved
    // session whose parameters came back with it.
    void loadFile(const juce::File& file, int osc = 0, bool waveSettings = true);
    // Message thread.  A V-Synth patch (.p00): zone 1 to the parameters and the effects, and the waves it names, from the Wave folder beside its own; zones 2…16 as the
    // patch's data, each its own settings and waves, which notes play by the upper keys (the user's decision, 2026-11-05: the parameters and editor stay on zone 1).  False,
    // and the status says why, if it's no patch.  The effects sound only with the Effects switch on, which a patch leaves as it is.  The name, and what it asks for that isn't
    // here (XtPatch.h), for the editor.  applyPatch's `zone` is the one put on the parameters, 0 but in PluginTest (each zone's values through them).
    bool loadPatch(const juce::File& file, int zone = 0);
    void applyPatch(const std::string& name, const std::vector<uint8_t>& store, int zone, const juce::File& waveFolder);
    // How many zones the patch loaded last plays (1 for none), and whether its zones' waves are all encoded and in the synth.
    int patchZones() const { return patchZones_.load(); }
    bool zonesReady() const { return zonesReady_.load(); }
    juce::String getPatchName() const { return patchName_; }
    juce::String getPatchNote() const { return patchNote_; }
    juce::File lastPatchFolder() const;
    XtSynth::Controls currentControls() const { return xtControls(); }

    juce::String getLoadedFileName(int osc = 0) const  { return wave_[osc].name; }
    bool         hasFileLoaded(int osc = 0)    const   { return wave_[osc].loaded.load(); }

    // ── The XT's firmware (rom.2) ────────────────────────────────────────────
    // Message thread.  The image the user chose: used now and kept for every project and plugin format, so a new
    // instance finds it (findRom).  Returns false, with the last image still in use, if the file is not a usable image.
    bool setRomFile(const juce::File& file);
    bool hasRom() const { return xtSynthRomReady(); }

    // ── The effects ────────────────────────────────────
    // What the control thread does each time round, also callable from a test: with the Effects switch on and rom.2 here, the chip and its driver come up (the first time) and take
    // what's changed.  Never on the audio thread.
    void fxTick();
    bool effectsRunning() const { return fxRunning_.load(); }
    void fxManualTicks();                           // for PluginTest: no thread, so fxTick falls where it says
    // The effects' parameters that aren't the host's to automate: those whose meaning hangs on a type, and the system's EQ.  By the firmware's ID, any thread.  fxParam: `fallback` if it hasn't been set.
    void setFxParam(int id, int value);
    int fxParam(int id, int fallback) const;
    // Message thread: names and ranges, once rom.2 is here (ok false till then).
    const XtFxCatalog& fxCatalog();
    // The firmware's ID behind one of the plugin's effect parameters, or -1.
    static int fxKnobId(const juce::String& parameter);
    // One line for the editor: ROM, encode progress, current encode type.
    juce::String getStatusText() const;

    juce::AudioFormatManager& getFormatManager() { return formatManager_; }

private:
    VSE::VariphraseEngine engine_;
    XtSynth synth_;
    std::atomic<bool> xtActive_ { false };      // the XT voices own the output
    std::vector<float> scratch_;                // right channel for a mono output bus
    int tripKnob_ = 0;                          // the Time Trip knob as XtSynth last had it

    // The effect chip and its driver, made the first time the Effects switch is on.  They cost a core's share and some memory, so not before.
    class FxThread;
    std::unique_ptr<FxThread> fxThread_;
    std::unique_ptr<XtFxUnit> fxUnit_;
    std::mutex fxLock_;                         // one control thread at a time: its own, or a test's
    juce::String romFile_;                      // under fxLock_
    std::atomic<bool> fxRunning_ { false };
    bool fxManual_ = false;
    std::atomic<bool> fxPatchPending_ { false };  // a saved state came back: all of it, as a patch change
    static constexpr int kFxIds = 0x8d0, kFxNoEdit = INT_MIN;
    std::atomic<int> fxEdits_[kFxIds];
    void fxApply(XtFxControl& c);
    bool effectsWanted() const;                 // the Effects switch
    XtFxCatalog fxCatalog_;

    juce::AudioFormatManager formatManager_;

    // ── File buffers, one per OSC ────────────────────────────────────────────
    // Written on the message thread, OSC 1's read on the audio thread (legacy path).
    struct LoadedWave {
        juce::AudioBuffer<float> buffer;
        std::atomic<bool>        loaded { false };
        juce::String             name;
        double                   rate = 44100.0;
        int                      bits = 16;
        int                      ownType = -1;       // a V-Synth wave's own encode type (XtVpwFormat): 0 LITE, 1 SOLO, 3 BACKING, 5 ENSEMBLE; -1 none
        bool                     isOwn = false;      // a V-Synth wave, and what it says of itself (its audio left out)
        XtWaveFile               own;
        std::atomic<int>         gen { 0 };          // a newer encode request discards an older result
        std::atomic<bool>        reencode { false }; // its Encode Type or Depth changed
    };
    LoadedWave               wave_[2];
    juce::CriticalSection    bufferLock_;
    // A patch's zones 2…16: their settings at once, and again with their waves once the pool has encoded them (each wave once, as its own
    // type).  A newer patch discards an older one's.  The store and the wave folder go in the session, for its zones to come back.
    void loadZones(const std::vector<uint8_t>& store, const juce::File& waveFolder, juce::StringArray& notes);
    void restoreZones(juce::StringArray& notes);   // a session's zones back (PluginState.cpp), what isn't there in notes
    std::atomic<int>         zonesGen_ { 0 }, patchZones_ { 1 };
    std::atomic<bool>        zonesReady_ { true };

    // ── Encoding (background) ────────────────────────────────────────────────
    juce::ThreadPool encodePool_ { 1 };
    void addEncode(std::function<void()> job);   // a job on the pool whose firmware stops when the pool asks it to (v168)
    mutable juce::CriticalSection statusLock_;
    juce::String status_;
    void reencode(int osc);
    void setStatus(const juce::String& s);
    void handleAsyncUpdate() override;          // re-encode after an Encode Type / Depth change
    void findRom();
    bool openRom(const juce::File& file);       // for this instance, and kept nowhere but its project

    // ── Playback state (audio thread, legacy path) ───────────────────────────
    int  playbackPos_  = -1;
    int  activeVoices_ = 0;

    VSE::VariphraseParams engineParams() const;   // the legacy engine's, from the parameters
    XtSynth::Controls xtControls() const;
    void setControls(const XtSynth::Controls& c);
    void waveToParameters(const XtWaveFile& wave, int osc);
    void takeWave(const XtWaveFile& wave, const juce::File& file, int osc);
    void unloadWave(int osc);
    juce::String patchName_, patchNote_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VSynthEmuProcessor)
};
