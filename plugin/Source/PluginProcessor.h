#pragma once

#include <map>
#include <array>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "XtSynth.h"
#include "XtHostInput.h"
#include "XtFiles.h"
#include "XtFxControl.h"
#include "PluginArp.h"
#include "PluginLoadedWave.h"
#include "PluginMidiIn.h"
#include "XtSynthView.h"
#include "PluginVc2Play.h"
#include "PluginWaveView.h"
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
    // The outside input (PluginExtIn.cpp, v207): a stereo input bus beside the output, taken off, mono or stereo.
    // hasInput, any thread: whether the host gives it any channel, for the EXT IN OSC's word on the panel.
    bool isBusesLayoutSupported(const BusesLayout& layout) const override;
    bool hasInput() const;
    int inputLatency() const { return hostIn_.latency(); }   // XtHostInput's, in frames at 44.1 kHz, for the check
    long inputFaults() const { return hostIn_.shortBlocks() + hostIn_.overruns(); }   // and what went wrong in it

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

    // ── File loading ─────────────────────────────────────────────────────────
    // Message thread.  Reads the WAV/AIFF and encodes it in the background
    // as the selected Encode Type (the XT path needs the firmware image).  Each
    // OSC has its own wave (osc: 0 OSC 1, 1 OSC 2), as on the XT; without the
    // firmware, OSC 1's plays as it is.
    // A V-Synth wave (.vpw, or a project's .wav) brings its own Original Key, Tempo, loop and fine tuning to OSC's parameters unless waveSettings is false, as for a saved
    // session whose parameters came back with it.
    void loadFile(const juce::File& file, int osc = 0, bool waveSettings = true);
    // Message thread.  A V-Synth patch (.p00): zone 1 to the parameters and the effects, and the waves it names, from the Wave folder beside its own; zones 2…16 as the
    // patch's data, each its own settings and waves, which notes play by the upper keys (the user's decision, 2026-11-05: the parameters and editor stay on zone 1).  False,
    // and the status says why, if it's no patch.  The effects sound only with the Effects switch on, which a patch leaves as it is.  The name, and what it asks for that isn't
    // here (XtPatch.h), for the editor.  applyPatch's `zone` is the one put on the parameters, 0 but in PluginTest (each zone's values through them).
    bool loadPatch(const juce::File& file, int zone = 0);
    // Message thread.  A VC-2 patch (.p02, v452; PluginVc2Patch.h): rom.4's reader's values onto the VC-2's parameters, its
    // store kept for the session; false, the status saying why, for a file that isn't one.  The store and name kept last.
    bool loadVc2Patch(const juce::File& file);
    std::vector<uint8_t> vc2PatchStore() const;
    juce::String vc2PatchName() const;
    void applyPatch(const std::string& name, const std::vector<uint8_t>& store, int zone, const juce::File& waveFolder);
    // How many zones the patch loaded last plays (1 for none), and whether its zones' waves are all encoded and in the synth.
    int patchZones() const { return patchZones_.load(); }
    bool zonesReady() const { return zonesReady_.load(); }
    juce::String getPatchName() const { return patchName_; }
    juce::String getPatchNote() const { return patchNote_; }
    juce::File lastPatchFolder() const;
    std::vector<uint8_t> keptStore() const;   // the loaded patch's store, kept in the session (PluginZones.cpp); a save's
    XtSynth::Controls currentControls() const { return xtControls(); }
    // What zones 2…16 of the patch loaded last play, a line each, the first zone 1's and empty; none without a patch
    // (PluginZones.cpp, v195).  Message thread
    juce::StringArray zonePlays() const;
    // The arpeggio's pattern (PluginArp.cpp, v190): a patch's, or a session's; any thread reads it, the message thread sets it
    XtArpPattern arpPattern() const { return arpWords_.load(); }
    void setArpPattern(const XtArpPattern& p);

    juce::String getLoadedFileName(int osc = 0) const  { return wave_[osc].name; }
    bool         hasFileLoaded(int osc = 0)    const   { return wave_[osc].loaded.load(); }
    // A loaded wave as the editor draws it (PluginWaveView.h, v200).  None before one has.  Message thread, as is gen.
    using WaveView = PluginWaveView;
    std::shared_ptr<const WaveView> waveView(int osc) const;
    int waveViewGen(int osc) const { return waveViewGen_[osc].load(); }   // moves with each view made

    // ── The plugin's MIDI ahead of the firmware's (PluginMidiIn.h, v462): the editor's notes and controllers (v201), a
    // control change's edits of the patch onto the parameters ─────────
    bool sendFromEditor(const juce::MidiMessage& m) { return midi_.send(m); }
    template <class Take>
    void takeFromEditor(Take&& take) { midi_.take(take); }
    PluginMidiIn& midiIn() { return midi_; }
    const XtSynthView& modulation() const { return modView_; }   // v464: the newest note's, for the knobs' rings

    // ── The XT's firmware (rom.2) ────────────────────────────────────────────
    // Message thread.  The image the user chose: used now and kept for every project and plugin format, so a new
    // instance finds it (findRom).  Returns false, with the last image still in use, if the file is not a usable image.
    bool setRomFile(const juce::File& file);
    bool hasRom() const { return xtSynthRomReady(); }
    // ── The VC-2's firmware (rom.4, v198) ──
    // Message thread.  Found once rom.2 is open, beside it first (findVc2Rom), or chosen here and kept as rom.2's is.
    // Returns false, the status saying why, if the file isn't the VC-2's image.  No port reads it yet (the VC-2 mode).
    bool setVc2RomFile(const juce::File& file);
    bool hasVc2Rom() const { return xtSynthVc2RomReady(); }
    PluginVc2Play& vc2Play() { return vc2Play_; }   // the VC-2 mode's audio (v455), its tick a test's to run by hand

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
    static const char* fxKnobOf(int id);   // and the other way: the parameter for a firmware ID, or nullptr (v207)
    static constexpr int kAfterZones = 2;   // the parameters appended after the zones' (v206, External In Type and Source)
    // One line for the editor: ROM, encode progress, current encode type.
    juce::String getStatusText() const;

    juce::AudioFormatManager& getFormatManager() { return formatManager_; }

private:
    XtSynth synth_;
    XtHostInput hostIn_;                        // the outside input on its way to synth_'s EXT IN OSCs, its feed (v207)
    // processBlock's parts, the audio thread's (v207): the input's channels, the rest cleared, and External In Type to
    // hostIn_ (PluginExtIn.cpp); whether an OSC plays with no wave; the MIDI at its frames with the render between
    // (PluginRender.cpp); and frames [from, to) of the input in the buffer's first `inputs` channels into hostIn_, then
    // synth_'s render of them into L and R, which may be those channels: each span read before it's written
    int inputBlock(juce::AudioBuffer<float>& buffer);
    bool playsWaveless() const;
    void playMidi(const juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi, int inputs, float* L, float* R);
    void renderSpan(const juce::AudioBuffer<float>& in, int inputs, float* L, float* R, int from, int to);
    // The parameters those read each block, looked up once (PluginRender.cpp)
    struct BlockReads {
        explicit BlockReads(juce::AudioProcessorValueTreeState& s);
        std::atomic<float>* oscType;
        std::atomic<float>* osc2Switch;
        std::atomic<float>* extInType;
    };
    const BlockReads reads_ { apvts };
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

    // ── File buffers, one per OSC (PluginLoadedWave.h, v452) ─────────────────
    using LoadedWave = PluginLoadedWave;
    LoadedWave               wave_[2];
    juce::CriticalSection    bufferLock_;
    // Each OSC's wave as the editor draws it (PluginWaveView.cpp, v200), made from the channels its encode copies; the
    // lock is the message thread's alone, never the audio thread's
    void publishWaveView(int osc, const std::vector<std::vector<float>>& chans);
    mutable juce::CriticalSection viewLock_;
    std::shared_ptr<const WaveView> waveView_[2];
    std::atomic<int> waveViewGen_[2] {};
    PluginMidiIn midi_ { apvts };   // v462: the editor's queue and the MIDI edits (PluginMidiIn.h)
    XtSynthView modView_;           // v464: taken after each block the XT's players play, none sounding otherwise
    void toSynth(const juce::MidiMessage& msg);   // one message to the synth, the host's or the editor's
    void toFile(const juce::MidiMessage& msg);    // or to the file played without the firmware
    void playFile(juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi, int numOut);   // that file's block
    // A patch's zones 2…16: their settings at once, and again with their waves once the pool has encoded them (each wave once, as its own
    // type).  A newer patch discards an older one's.  The store and the wave folder go in the session, for its zones to come back.
    // PluginZones.cpp (v195), with the splits' parameters
    void loadZones(const std::vector<uint8_t>& store, const juce::File& waveFolder, juce::StringArray& notes);
    // a session's zones back, what isn't there in notes; and its splits from its store if it has none (before v195)
    void restoreZones(juce::StringArray& notes, bool hadSplits);
    void setZonePlays(const juce::StringArray& plays);
    std::atomic<int>         zonesGen_ { 0 }, patchZones_ { 1 };
    std::atomic<bool>        zonesReady_ { true };

    // ── Encoding (background) ────────────────────────────────────────────────
    juce::ThreadPool encodePool_ { 1 };
    // A job on the pool whose firmware stops when the pool asks it to (v168), given the stop for what isn't firmware (v185)
    void addEncode(std::function<void(const std::atomic<bool>& stop)> job);
    mutable juce::CriticalSection statusLock_;
    juce::String status_;
    void reencode(int osc);
    void encodeWave(int osc, int choice, int depth);   // the wave's encode on the pool, as Encode Type `choice` (PluginEncodes.cpp)
    void setStatus(const juce::String& s);
    void handleAsyncUpdate() override;          // re-encode after an Encode Type / Depth change
    void findRom();
    bool openRom(const juce::File& file);       // for this instance, and kept nowhere but its project
    void findVc2Rom(const juce::File& rom);     // PluginRom.cpp

    // ── Playback state (audio thread, legacy path) ───────────────────────────
    int  playbackPos_  = -1;
    int  activeVoices_ = 0;

    XtSynth::Controls xtControls() const;
    void setControls(const XtSynth::Controls& c);
    // The parameters' layout by part, in the order a host has always seen them (PluginParams*.cpp, v190)
    using ParamList = std::vector<std::unique_ptr<juce::RangedAudioParameter>>;
    using IntText = juce::AudioParameterIntAttributes;
    using FloatText = juce::AudioParameterFloatAttributes;
    static void addOscParams(ParamList& params, const FloatText& twoPlaces);
    static void addCosmParams(ParamList& params, const IntText& noteValue);
    static void addToneParams(ParamList& params, const IntText& noteValue, const IntText& curveName);
    static void addOsc2Params(ParamList& params, const IntText& noteValue, const FloatText& twoPlaces,
                              const IntText& curveName);
    static void addModParams(ParamList& params);
    // xtControls and setControls by part (PluginControlsRead.cpp, PluginControlsWrite.cpp, PluginCosmControls.cpp,
    // PluginOsc2Controls.cpp, v190)
    float raw(const juce::String& id) const;         // a parameter's value as the synth reads it (v462: a MIDI edit's)
    int knob(const juce::String& id) const;          // a parameter's value, rounded
    int kf(const juce::String& id) const;            // a keyfollow's, to its step of 10
    void readOscControls(XtSynth::Controls& c) const;
    void readCosmControls(XtSynth::Controls& c) const;
    void readToneControls(XtSynth::Controls& c) const;
    void readOsc2Controls(XtSynth::Controls& c) const;
    void readEnvVelocity(XtSynth::Controls& c) const;
    void set(const juce::String& id, float v);       // as a host's automation would, and only if it has moved
    void writeOscControls(const XtSynth::Controls& c);
    void writeCosmControls(const XtSynth::Controls& c);
    void writeToneControls(const XtSynth::Controls& c);
    void writeOsc2Controls(const XtSynth::Controls& c);
    // The arpeggio's ten settings, after every other parameter, and its pattern (PluginArp.cpp, v190)
    static void addArpParams(ParamList& params);
    void readArpControls(XtSynth::Controls& c) const;
    void writeArpControls(const XtSynth::Controls& c);
    void restoreArpPattern();                        // a session's, or the XT's default (setStateInformation)
    ArpPatternWords arpWords_;
    // The Multi Step Modulator's 98 settings, after the arpeggio's (PluginMsm.cpp, v193); and a patch's controls, which
    // stop it as the XT's patch load does
    static void addMsmParams(ParamList& params);
    void readMsmControls(XtSynth::Controls& c) const;
    void writeMsmControls(const XtSynth::Controls& c);
    // The zones' sixteen Range Upper keys, after the MSM's (PluginZones.cpp, v195)
    static void addZoneParams(ParamList& params);
    static void addExtInParams(ParamList& params);   // PluginExtIn.cpp, v206: External In Type and Source
    void readZoneControls(XtSynth::Controls& c) const;
    void writeZoneControls(const XtSynth::Controls& c);
    void setPatchControls(const XtSynth::Controls& c);
    void waveToParameters(const XtWaveFile& wave, int osc);
    void takeWave(const XtWaveFile& wave, const juce::File& file, int osc);
    void unloadWave(int osc);
    juce::String patchName_, patchNote_;
    PluginVc2Patch vc2Patch_;   // the VC-2 patch loaded last, or a session's (v452)
    PluginVc2Play vc2Play_ { apvts, vc2Patch_ };   // the VC-2 mode's part and the patch it plays (v455)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VSynthEmuProcessor)
};
