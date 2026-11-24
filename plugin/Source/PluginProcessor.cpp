#include "PluginProcessor.h"
#include "UiEditor.h"
#include "PluginPatchFiles.h"
#include "PluginParamIds.h"
#include "XtPatch.h"
#include "XtVpwFormat.h"

// ─── Constructor / Destructor ─────────────────────────────────────────────────

VSynthEmuProcessor::VSynthEmuProcessor()
    // Instrument: a stereo input for the EXT IN OSCs (v207; a host may turn it off), and a stereo output
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "VSynthEmu", createParameterLayout())
{
    for (auto& e : fxEdits_) e.store(kFxNoEdit, std::memory_order_relaxed);
    formatManager_.registerBasicFormats();   // WAV, AIFF, OGG, FLAC
    formatManager_.registerFormat(new XtVpwFormat(), false);

    for (const auto& id : Params::encodeParamIds())
        apvts.addParameterListener(id, this);
    findRom();
}

// The control thread: the driver is quick, a hundred times a second is plenty, and nothing it does holds up the audio.
class VSynthEmuProcessor::FxThread : public juce::Thread {
public:
    explicit FxThread(VSynthEmuProcessor& p) : juce::Thread("XT effects"), owner(p) {}
    void run() override { while (!threadShouldExit()) { owner.fxTick(); wait(10); } }
private:
    VSynthEmuProcessor& owner;
};

// fxTick, the effects' parameters by the firmware's ID and the catalog are in PluginEffects.cpp (v172, v207)
void VSynthEmuProcessor::fxManualTicks() {
    fxManual_ = true;
    if (fxThread_) fxThread_->stopThread(2000);
    fxThread_.reset();
}

VSynthEmuProcessor::~VSynthEmuProcessor() {
    if (fxThread_) fxThread_->stopThread(2000);
    fxThread_.reset();
    for (const auto& id : Params::encodeParamIds())
        apvts.removeParameterListener(id, this);
    encodePool_.removeAllJobs(true, -1);   // no deadline: each job hears the stop and ends (PluginEncodes.cpp, v185)
}

// The firmware image's search and the plugin's own folder are in PluginRom.cpp and PluginAppFolder.cpp (v198)

// ─── Lifecycle ────────────────────────────────────────────────────────────────

void VSynthEmuProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    synth_.prepare(sampleRate);
    hostIn_.prepare(sampleRate, synth_.input);   // the outside input at this rate, feeding the EXT IN OSCs (v207)
    scratch_.assign((size_t)std::max(samplesPerBlock, 1) * 2, 0.0f);
    if (!fxThread_ && !fxManual_) { fxThread_ = std::make_unique<FxThread>(*this); fxThread_->startThread(); }
    playbackPos_  = -1;
    activeVoices_ = 0;
}

void VSynthEmuProcessor::releaseResources() {}

// ─── File Loading ─────────────────────────────────────────────────────────────
// A V-Synth wave known by its first chunks: isVSynthWave, PluginPatchFiles.cpp (v600)

// The wave's own settings to OSC `osc`'s parameters.
void VSynthEmuProcessor::waveToParameters(const XtWaveFile& wave, int osc) {
    XtSynth::Controls c = xtControls();
    xtWaveOntoControls(wave, osc ? c.osc2 : static_cast<XtSynth::OscControls&>(c));
    const XtSynth::OscControls& o = osc ? c.osc2 : c;
    const auto set = [this, osc](const juce::String& id, float v) {
        auto* p = apvts.getParameter(Params::oscId(osc, id));
        if (p) p->setValueNotifyingHost(p->convertTo0to1(v));
    };
    set(Params::kRootKey, (float)o.rootKey);
    set(Params::kOrigTempo, (float)o.origTempo / 10000.0f);
    set(Params::kLoopStart, o.loopStart * 100.0f);
    set(Params::kLoopEnd, o.loopEnd * 100.0f);
    set(Params::kLoopMode, (float)o.loopMode);
    set(Params::kLoop, o.loop ? 1.0f : 0.0f);
}

// A V-Synth wave's audio into OSC `osc`.  What the wave says of itself goes with it to each encode.
void VSynthEmuProcessor::takeWave(const XtWaveFile& wave, const juce::File& file, int osc) {
    juce::AudioBuffer<float> audio(wave.channels, (int)wave.frames);
    for (int c = 0; c < wave.channels; ++c) {
        float* d = audio.getWritePointer(c);
        for (long i = 0; i < wave.frames; ++i) d[i] = (float)wave.pcm[(size_t)(i * wave.channels + c)] / 32768.0f;
    }
    {
        juce::CriticalSection::ScopedLockType lock(bufferLock_);
        LoadedWave& w = wave_[osc];
        w.buffer = std::move(audio);
        w.rate = 44100.0;
        w.bits = 16;
        w.ownType = wave.encodeType;
        w.own = wave;
        w.own.pcm.clear();
        w.isOwn = true;
        w.loaded.store(true);
    }
    if (osc == 0) { playbackPos_ = -1; activeVoices_ = 0; }
    wave_[osc].name = file.getFileName();
    apvts.state.setProperty(osc ? "loadedFilePath2" : "loadedFilePath", file.getFullPathName(), nullptr);
    reencode(osc);
}

void VSynthEmuProcessor::unloadWave(int osc) {
    LoadedWave& w = wave_[osc];
    {
        juce::CriticalSection::ScopedLockType lock(bufferLock_);
        w.loaded.store(false);
        w.buffer.setSize(0, 0);
        w.isOwn = false;
        w.ownType = -1;
    }
    ++w.gen;                       // an encode under way comes to nothing
    if (osc == 0) { playbackPos_ = -1; activeVoices_ = 0; }
    w.name.clear();
    apvts.state.setProperty(osc ? "loadedFilePath2" : "loadedFilePath", juce::String(), nullptr);
    synth_.setSample(nullptr, osc);
}

void VSynthEmuProcessor::loadFile(const juce::File& file, int osc, bool waveSettings)
{
    if (xtSynthRomReady() && isVSynthWave(file)) {
        XtWaveFile wave;
        std::string why;
        if (readVSynthWave(file, wave, &why)) {
            if (waveSettings) waveToParameters(wave, osc);
            takeWave(wave, file, osc);
            return;
        }
        setStatus(file.getFileName() + ": " + juce::String(why));
    }

    std::unique_ptr<juce::AudioFormatReader> reader(
        formatManager_.createReaderFor(file));

    if (!reader)
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Load Error",
            (file.hasFileExtension("vpw") && !xtSynthRomReady() ? "A .vpw needs the firmware image: " : "Could not read: ") + file.getFileName());
        return;
    }

    const int totalSamples = static_cast<int>(reader->lengthInSamples);
    const int numChans     = static_cast<int>(reader->numChannels);

    juce::AudioBuffer<float> newBuffer(numChans, totalSamples);
    reader->read(&newBuffer, 0, totalSamples, 0, true, true);

    {
        juce::CriticalSection::ScopedLockType lock(bufferLock_);
        LoadedWave& w = wave_[osc];
        w.buffer = std::move(newBuffer);
        w.rate = reader->sampleRate;
        w.bits = reader->usesFloatingPointData ? 16 : (int)reader->bitsPerSample;   // float WAVs: rounded
        w.ownType = reader->metadataValues.getValue(XtVpwFormat::kEncodeType, "-1").getIntValue();
        w.isOwn = false;
        w.loaded.store(true);
    }

    if (osc == 0) {
        playbackPos_  = -1;
        activeVoices_ = 0;
    }
    wave_[osc].name = file.getFileName();

    // Persist the file path so it survives DAW project reload
    apvts.state.setProperty(osc ? "loadedFilePath2" : "loadedFilePath", file.getFullPathName(), nullptr);
    reencode(osc);
}

// ─── A V-Synth patch ─────────────────────────────────────────────────────────
// Read from its file, its folder kept, in PluginPatchFiles.cpp (v198)

void VSynthEmuProcessor::applyPatch(const std::string& name, const std::vector<uint8_t>& store, int zone, const juce::File& waveFolder) {
    XtSynth::Controls c = xtWithPatchArp(store, xtPatchControls(store, zone, xtControls()));   // and its arpeggio (v190)
    // Its waves first, for what they say of themselves
    juce::File files[2];
    XtWaveFile waves[2];
    bool found[2] = {};
    juce::StringArray notes;
    for (int k = 0; k < 2; ++k) {
        XtSynth::OscControls& o = k ? c.osc2 : static_cast<XtSynth::OscControls&>(c);
        const int number = xtPatchWaveNumber(store, zone, k);
        if (o.oscType != 1 || number <= 0) continue;
        files[k] = findWave(waveFolder, number);
        std::string why;
        if (files[k].existsAsFile() && readVSynthWave(files[k], waves[k], &why)) { found[k] = true; xtWaveOntoControls(waves[k], o); }
        else notes.add("no wave " + juce::String(number) + (why.empty() ? juce::String() : ": " + juce::String(why)));
    }
    setPatchControls(c);   // and the Multi Step Modulator stopped, as the XT's patch load does (PluginMsm.cpp, v193)
    for (int k = 0; k < 2; ++k)
        if (auto* p = apvts.getParameter(Params::oscId(k, Params::kEncodeType))) p->setValueNotifyingHost(p->convertTo0to1(0.0f));   // Auto: each wave's own
    for (const auto& fx : xtPatchEffects(store)) {
        const char* knob = fxKnobOf(fx.id);
        if (!knob) { setFxParam(fx.id, fx.value); continue; }
        if (auto* p = apvts.getParameter(knob)) p->setValueNotifyingHost(p->convertTo0to1((float)fx.value));
    }
    fxPatchPending_ = true;
    for (int k = 0; k < 2; ++k) {
        if (found[k]) takeWave(waves[k], files[k], k);
        else unloadWave(k);
    }
    loadZones(store, waveFolder, notes);
    for (int z = 0; z < xtPatchZones(store); ++z)
        for (const auto& m : xtPatchMissing(store, z)) notes.addIfNotAlreadyThere(m);
    patchName_ = juce::String(name);
    patchNote_ = notes.joinIntoString(", ");
    apvts.state.setProperty("patchName", patchName_, nullptr);
    apvts.state.setProperty("patchNote", patchNote_, nullptr);
    if (!found[0] && !found[1]) setStatus("Patch: " + patchName_);
}

// ─── Encoding ─────────────────────────────────────────────────────────────────

void VSynthEmuProcessor::setStatus(const juce::String& s) {
    const juce::ScopedLock l(statusLock_);
    status_ = s;
}

juce::String VSynthEmuProcessor::getStatusText() const {
    const juce::ScopedLock l(statusLock_);
    return status_;
}

// Encodes the loaded file as the XT would (XtSynth.h) on the pool thread;
// a newer request discards an older result.
void VSynthEmuProcessor::reencode(int osc) {   // the encode itself is in PluginEncodes.cpp (v185)
    const int choice = (int)apvts.getRawParameterValue(Params::oscId(osc, Params::kEncodeType))->load();
    const int depth  = (int)apvts.getRawParameterValue(Params::oscId(osc, Params::kEncodeDepth))->load();
    encodeWave(osc, choice, depth);
}

void VSynthEmuProcessor::handleAsyncUpdate() {
    for (int k = 0; k < 2; ++k)
        if (wave_[k].reencode.exchange(false)) reencode(k);
}

// ─── Parameters ───────────────────────────────────────────────────────────────

void VSynthEmuProcessor::parameterChanged(const juce::String& paramID, float /*newValue*/) {
    for (int k = 0; k < 2; ++k)
        if (paramID == Params::oscId(k, Params::kEncodeType) || paramID == Params::oscId(k, Params::kEncodeDepth)) {
            wave_[k].reencode.store(true);
            triggerAsyncUpdate();
            return;
        }
}

// The state, getStateInformation and setStateInformation, is in PluginState.cpp (v172).

// ─── Editor / Entry Point ─────────────────────────────────────────────────────

juce::AudioProcessorEditor* VSynthEmuProcessor::createEditor() {
    return new SynthEditor(*this);   // D (research/UI_DESIGN.md), from v196
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new VSynthEmuProcessor();
}

// ── The Matrix's lists ──────────────────────────────────────────────────
juce::StringArray VSynthEmuProcessor::matrixSourceNames() {
    juce::StringArray a { "OFF" };
    for (int n = 1; n < 96; ++n) a.add(n == 32 ? juce::String("(OFF)") : "CC" + juce::String(n).paddedLeft('0', 2));
    a.addArray(juce::StringArray { "BEND", "AFT", "+PAD-X", "+PAD-Y", "PAD-X", "PAD-Y", "TRIP-R", "BEAM-L", "BEAM-R", "KNOB1", "KNOB2", "VELO", "KEYF" });
    return a;
}

juce::StringArray VSynthEmuProcessor::matrixDestNames() {
    juce::StringArray a { "OFF" };
    for (const char* o : { "OSC1", "OSC2" }) {
        for (const char* d : { "PITCH", "TIME/PW", "FORMA/FAT", "LVL", "PENV-ATK", "PENV-DCY", "PENV-REL", "TENV-ATK", "TENV-DCY", "TENV-REL",
                               "FENV-ATK", "FENV-DCY", "FENV-REL", "AENV-ATK", "AENV-DCY", "AENV-REL", "LFO-RATE", "LFO-PCH", "LFO-TM/PW", "LFO-FR/FT", "LFO-LVL" })
            a.add(juce::String(o) + "-" + d);
    }
    for (const char* o : { "CSM1", "CSM2" })
        for (const char* d : { "PRM1", "PRM2", "ENV1-ATK", "ENV1-DCY", "ENV1-REL", "ENV2-ATK", "ENV2-DCY", "ENV2-REL", "LFO-RATE", "LFO-PRM1", "LFO-PRM2" })
            a.add(juce::String(o) + "-" + d);
    a.addArray(juce::StringArray { "TVA-LVL", "TVA-ENV-ATK", "TVA-ENV-DCY", "TVA-ENV-REL", "TVA-LFO-RATE", "TVA-LFO-LVL", "TVA-LFO-PAN", "MFX-SEND", "CHO-SEND", "REV-SEND",
                                   "MFX-PRM1", "MFX-PRM2", "MFX-PRM3", "TVA-PAN" });
    return a;
}

juce::StringArray VSynthEmuProcessor::assignValueNames() {
    juce::StringArray a { "OFF" };
    for (int n = 1; n < 96; ++n) a.add(n == 32 ? juce::String("(OFF)") : "CC" + juce::String(n).paddedLeft('0', 2));
    return a;
}

const char* VSynthEmuProcessor::assignParamId(int i) {
    static const char* const k[kAssigns] = { "assignBeam2L", "assignBeam2R", "assignBeam3L", "assignBeam3R", "assignBeam4L", "assignBeam4R", "assignPadX", "assignPadY", "assignKnob1", "assignKnob2", "assignTripX", "assignTripY" };
    return k[i];
}

const char* VSynthEmuProcessor::assignName(int i) {
    static const char* const k[kAssigns] = { "Beam 2 L", "Beam 2 R", "Beam 3 L", "Beam 3 R", "Beam-L", "Beam-R", "Pad X", "Pad Y", "Knob 1", "Knob 2", "Trip X", "Trip Y" };
    return k[i];
}

int VSynthEmuProcessor::assignDefault(int i) { return i == 6 ? 80 : i == 7 ? 81 : i == 10 ? 82 : i == 11 ? 83 : 0; }
