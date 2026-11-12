#include "PluginProcessor.h"
#include <array>
#include <map>
#include "PluginEditor.h"
#include "PluginParamIds.h"
#include "XtPatch.h"
#include "XtVpwFormat.h"


// ─── Constructor / Destructor ─────────────────────────────────────────────────

VSynthEmuProcessor::VSynthEmuProcessor()
    // Instrument: stereo output only, no sidechain input
    : AudioProcessor(BusesProperties()
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "VSynthEmu", createParameterLayout())
{
    for (auto& e : fxEdits_) e.store(kFxNoEdit, std::memory_order_relaxed);
    formatManager_.registerBasicFormats();   // WAV, AIFF, OGG, FLAC
    formatManager_.registerFormat(new XtVpwFormat(), false);

    for (const auto& id : { Params::kPitchShift, Params::kTimeStretch, Params::kFormantShift, Params::kRobot,
                            Params::kEncodeType, Params::kEncodeDepth, Params::oscId(1, Params::kEncodeType), Params::oscId(1, Params::kEncodeDepth) })
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

// fxTick is in PluginEffects.cpp (v172)
bool VSynthEmuProcessor::effectsWanted() const { return apvts.getRawParameterValue(Params::kFxOn)->load() >= 0.5f; }

// The plugin's effect parameters, each the firmware's ID.
static const struct { const char* name; int id; } kFxKnobs[] = {
    { "fxMfxOn", 0x120 }, { "fxChoOn", 0x121 }, { "fxRevOn", 0x122 },
    { "fxMfxType", 0x177 }, { "fxMfxMaster", 0x178 }, { "fxMfxCho", 0x179 }, { "fxMfxRev", 0x17a },
    { "fxChoType", 0x1fc }, { "fxChoMaster", 0x1fd }, { "fxChoRev", 0x1fe },
    { "fxRevType", 0x230 }, { "fxRevMaster", 0x231 },
    { "fxMaster", 0x3e }, { "fxGain", 0x76 }
};

void VSynthEmuProcessor::fxManualTicks() {
    fxManual_ = true;
    if (fxThread_) fxThread_->stopThread(2000);
    fxThread_.reset();
}

int VSynthEmuProcessor::fxKnobId(const juce::String& parameter) {
    for (const auto& k : kFxKnobs) if (parameter == k.name) return k.id;
    return -1;
}

// All of it to the driver's store, which only takes what's changed.
void VSynthEmuProcessor::fxApply(XtFxControl& c) {
    for (const auto& k : kFxKnobs) c.set(k.id, (int)std::lround(apvts.getRawParameterValue(k.name)->load()));
    c.set(0x11d, synth_.fxPatchTempo());
    c.set(0x41, synth_.fxClockSix() ? 1 : 0);
    for (int id = 0; id < kFxIds; ++id) { const int v = fxEdits_[id].load(std::memory_order_relaxed); if (v != kFxNoEdit) c.set(id, v); }
}

void VSynthEmuProcessor::setFxParam(int id, int value) { if (id >= 0 && id < kFxIds) fxEdits_[id].store(value, std::memory_order_relaxed); }

int VSynthEmuProcessor::fxParam(int id, int fallback) const {
    const int v = id >= 0 && id < kFxIds ? fxEdits_[id].load(std::memory_order_relaxed) : kFxNoEdit;
    return v == kFxNoEdit ? fallback : v;
}

const XtFxCatalog& VSynthEmuProcessor::fxCatalog() {
    if (!fxCatalog_.ok) {
        juce::String rom;
        { const std::lock_guard<std::mutex> lock(fxLock_); rom = romFile_; }
        if (rom.isNotEmpty()) fxCatalog_ = xtFxCatalog(rom.toRawUTF8());
    }
    return fxCatalog_;
}

VSynthEmuProcessor::~VSynthEmuProcessor() {
    if (fxThread_) fxThread_->stopThread(2000);
    fxThread_.reset();
    for (const auto& id : { Params::kPitchShift, Params::kTimeStretch, Params::kFormantShift, Params::kRobot,
                            Params::kEncodeType, Params::kEncodeDepth, Params::oscId(1, Params::kEncodeType), Params::oscId(1, Params::kEncodeDepth) })
        apvts.removeParameterListener(id, this);
    encodePool_.removeAllJobs(true, -1);   // no deadline: each job hears the stop and ends (PluginEncodes.cpp, v185)
}

// ─── The firmware image ───────────────────────────────────────────────────────

static const char* const kRomName = "V-SynthXT.rom.2.bin";   // rom.2 as research/ghidra_scripts/unpack_firmware.py writes it
static const char* const kRomKey  = "romPath";
static const char* const kPatchFolderKey = "patchFolder";

// What belongs to the plugin and not to a project, whichever format is running: ~/Library/Application Support/V-Synth Emu,
// or VSE_APP_DIR, so that PluginTest leaves the user's alone.
static juce::File appDir() {
    if (const char* d = std::getenv("VSE_APP_DIR")) return juce::File(juce::String::fromUTF8(d));
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Application Support/V-Synth Emu");
}

static std::unique_ptr<juce::PropertiesFile> appSettings() {
    juce::PropertiesFile::Options o;
    o.millisecondsBeforeSaving = -1;   // only when told
    return std::make_unique<juce::PropertiesFile>(appDir().getChildFile("settings.xml"), o);
}

// A new instance's search, first to last: VSE_XT_ROM; beside the plugin, never in its bundle, where an added file breaks
// the signature; the plugin's own folder; the image last chosen by hand, which is how an AU, a VST3 and a standalone, each
// in a folder of its own, share one.
void VSynthEmuProcessor::findRom() {
    juce::StringArray cand;
    if (const char* p = std::getenv("VSE_XT_ROM")) cand.add(juce::String::fromUTF8(p));
    cand.add(juce::File::getSpecialLocation(juce::File::currentApplicationFile).getSiblingFile(kRomName).getFullPathName());
    cand.add(appDir().getChildFile(kRomName).getFullPathName());
    cand.add(appSettings()->getValue(kRomKey));
    for (const auto& c : cand)
        if (c.isNotEmpty() && juce::File(c).existsAsFile() && openRom(juce::File(c))) return;
    setStatus("No ROM: choose V-SynthXT.rom.2.bin, or put it next to the plugin");
}

bool VSynthEmuProcessor::openRom(const juce::File& file) {
    if (std::string why; !file.existsAsFile() || !xtSynthSetRom(file.getFullPathName().toStdString(), &why)) {
        setStatus("Not a usable rom.2 image: " + file.getFileName() + (why.empty() ? "" : ": ") + juce::String(why));
        return false;
    }
    apvts.state.setProperty(kRomKey, file.getFullPathName(), nullptr);
    { const std::lock_guard<std::mutex> lock(fxLock_); romFile_ = file.getFullPathName(); }
    setStatus("ROM ready");
    reencode(0);
    reencode(1);
    return true;
}

bool VSynthEmuProcessor::setRomFile(const juce::File& file) {
    if (!openRom(file)) return false;
    appDir().createDirectory();
    auto settings = appSettings();
    settings->setValue(kRomKey, file.getFullPathName());
    settings->save();
    return true;
}

// ─── Lifecycle ────────────────────────────────────────────────────────────────

void VSynthEmuProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    engine_.prepare(sampleRate, samplesPerBlock, VSE::Algorithm::Passthrough, engineParams());
    setLatencySamples(engine_.getLatencySamples());
    synth_.prepare(sampleRate);
    scratch_.assign((size_t)std::max(samplesPerBlock, 1) * 2, 0.0f);
    if (!fxThread_ && !fxManual_) { fxThread_ = std::make_unique<FxThread>(*this); fxThread_->startThread(); }
    playbackPos_  = -1;
    activeVoices_ = 0;
}

void VSynthEmuProcessor::releaseResources() {
    engine_.reset();
}

// ─── Process ──────────────────────────────────────────────────────────────────

void VSynthEmuProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                       juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const int numOut     = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    // ── The XT's own players: one voice per note, sample-accurate MIDI ───────
    // (an analog OSC needs the firmware image but no sample)
    const bool analog = xtSynthRomReady() && (apvts.getRawParameterValue(Params::kOscType)->load() < 0.5f
                                              || apvts.getRawParameterValue(Params::oscId(1, Params::kOscSwitch))->load() >= 0.5f);
    if ((xtActive_.load() || analog) && numOut > 0) {
        synth_.setControls(xtControls());
        synth_.useBusSink(apvts.getRawParameterValue(Params::kFxOn)->load() >= 0.5f);
        // The host's transport, for Clock Source HOST (the Standalone has none: the Patch Tempo stands)
        double hostBpm = 0.0, hostPpq = 0.0;
        bool hostMoving = false;
        if (auto* head = getPlayHead())
            if (const auto info = head->getPosition()) {
                hostBpm = info->getBpm().orFallback(0.0);
                if (const auto ppq = info->getPpqPosition()) { hostPpq = *ppq; hostMoving = info->getIsPlaying(); }
            }
        synth_.hostClock(hostBpm, hostMoving, hostPpq);
        const int trip = (int)apvts.getRawParameterValue(Params::kTripPad)->load();
        if (trip != tripKnob_) { tripKnob_ = trip; synth_.timeTripKnob(trip); }
        float* L = buffer.getWritePointer(0);
        float* R = numOut > 1 ? buffer.getWritePointer(1) : nullptr;
        if (!R) {
            if ((int)scratch_.size() < numSamples) scratch_.resize((size_t)numSamples);   // normally sized in prepareToPlay
            R = scratch_.data();
        }
        int pos = 0;
        for (const auto meta : midi) {
            const int at = std::clamp(meta.samplePosition, pos, numSamples);
            if (at > pos) { synth_.render(L + pos, R + pos, at - pos); pos = at; }
            const auto msg = meta.getMessage();
            if (msg.isNoteOn()) synth_.noteOn(msg.getNoteNumber(), msg.getFloatVelocity());
            else if (msg.isNoteOff()) synth_.noteOff(msg.getNoteNumber(), msg.getFloatVelocity());
            else if (msg.isPitchWheel()) synth_.pitchBend(msg.getPitchWheelValue());
            else if (msg.isChannelPressure()) synth_.channelPressure(msg.getChannelPressureValue());
            else if (msg.isAftertouch()) synth_.polyPressure(msg.getNoteNumber(), msg.getAfterTouchValue());
            else if (msg.isController()) synth_.controller(msg.getControllerNumber(), msg.getControllerValue());   // the mode messages too
        }
        if (pos < numSamples) synth_.render(L + pos, R + pos, numSamples - pos);
        if (numOut == 1) for (int i = 0; i < numSamples; ++i) L[i] = 0.5f * (L[i] + R[i]);
        return;
    }

    // ── Without the firmware: the loaded file through the engine ─────────────
    // MIDI: note-on starts playback, note-off decrements voice count
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (msg.isNoteOn())
        {
            ++activeVoices_;
            playbackPos_ = 0;   // restart from top on any note-on
            engine_.reset();
        }
        else if (msg.isNoteOff())
        {
            activeVoices_ = std::max(0, activeVoices_ - 1);
            if (activeVoices_ == 0)
                playbackPos_ = -1;
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            activeVoices_ = 0;
            playbackPos_  = -1;
        }
    }

    if (playbackPos_ < 0 || !wave_[0].loaded.load())
        return;   // nothing to play — buffer is already silent

    // ── Grab the file buffer (non-blocking) ───────────────────────────────────
    juce::GenericScopedTryLock<juce::CriticalSection> lock(bufferLock_);
    if (!lock.isLocked())
        return;   // file is being swapped out — skip this block silently

    const int fileSamples = wave_[0].buffer.getNumSamples();
    const int fileChans   = wave_[0].buffer.getNumChannels();

    if (fileSamples == 0 || fileChans == 0)
        return;

    // ── Fill a mono source block from the file buffer ─────────────────────────
    // Mix file channels down to mono for the engine, then fan back out to stereo.
    juce::AudioBuffer<float> monoIn(1, numSamples);
    monoIn.clear();

    int remaining = numSamples;
    int written   = 0;

    while (remaining > 0 && playbackPos_ >= 0)
    {
        const int avail = fileSamples - playbackPos_;
        const int toCopy = std::min(avail, remaining);

        for (int ch = 0; ch < fileChans; ++ch)
            monoIn.addFrom(0, written,
                           wave_[0].buffer, ch,
                           playbackPos_, toCopy,
                           1.0f / float(fileChans));

        playbackPos_ += toCopy;
        written       += toCopy;
        remaining     -= toCopy;

        if (playbackPos_ >= fileSamples)
        {
            playbackPos_  = -1;   // reached end — stop
            activeVoices_ = 0;
            break;
        }
    }

    // ── Process mono through the engine ──────────────────────────────────────
    juce::AudioBuffer<float> monoOut(1, numSamples);
    monoOut.clear();

    const float* inPtr  = monoIn.getReadPointer(0);
    float*       outPtr = monoOut.getWritePointer(0);
    const float* inArr[1]  = { inPtr };
    float*       outArr[1] = { outPtr };
    engine_.process(inArr, outArr, 1, numSamples);

    // ── Fan mono output to all output channels ───────────────────────────────
    for (int ch = 0; ch < numOut; ++ch)
        buffer.copyFrom(ch, 0, monoOut, 0, 0, numSamples);
    buffer.applyGain(0.125f);   // at the level the XT's voices play (XtSynth::busOut), so losing the image isn't 18 dB louder
}

// ─── File Loading ─────────────────────────────────────────────────────────────

// A V-Synth wave by its first chunks: a .vpw, or a project's .wav, the same chunks in a RIFF.  A LITE wave in a project is a RIFF with no
// vpfl, its vpsc after the fmt chunk.
static bool isVSynthWave(const juce::File& file) {
    juce::FileInputStream in(file);
    char head[64] = {};
    const int n = in.openedOk() ? in.read(head, (int)sizeof head) : 0;
    if (n >= 4 && !std::memcmp(head, "vpfl", 4)) return true;
    if (n < 12 || std::memcmp(head, "RIFF", 4) || std::memcmp(head + 8, "WAVE", 4)) return false;
    for (int i = 12; i + 4 <= n; ++i) if (!std::memcmp(head + i, "vpfl", 4) || !std::memcmp(head + i, "vpsc", 4)) return true;
    return false;
}

static bool readVSynthWave(const juce::File& file, XtWaveFile& wave, std::string* why = nullptr) {
    juce::MemoryBlock bytes;
    return file.loadFileAsData(bytes) && xtWaveRead(static_cast<const uint8_t*>(bytes.getData()), bytes.getSize(), wave, why);
}

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

// A project keeps its waves in Wave beside Patch, each named by its number.  A copy named "… 2.wav" sorts after its original.
static juce::File findWave(const juce::File& folder, int number) {
    if (number <= 0 || !folder.isDirectory()) return {};
    juce::Array<juce::File> found;
    folder.findChildFiles(found, juce::File::findFiles, false, juce::String(number).paddedLeft('0', 3) + " *");
    juce::File best;
    for (const auto& f : found)
        if ((f.hasFileExtension("vpw") || f.hasFileExtension("wav")) && (best == juce::File() || f.getFileName().length() < best.getFileName().length()))
            best = f;
    return best;
}

bool VSynthEmuProcessor::loadPatch(const juce::File& file, int zone) {
    if (!xtSynthRomReady()) { setStatus("A patch needs the firmware image"); return false; }
    juce::MemoryBlock bytes;
    XtPatchFile patch;
    std::string why;
    if (!file.loadFileAsData(bytes) || !xtPatchRead(static_cast<const uint8_t*>(bytes.getData()), bytes.getSize(), patch, &why)) {
        setStatus(file.getFileName() + ": " + (why.empty() ? juce::String("can't be read") : juce::String(why)));
        return false;
    }
    appDir().createDirectory();
    auto settings = appSettings();
    settings->setValue(kPatchFolderKey, file.getParentDirectory().getFullPathName());
    settings->save();
    applyPatch(patch.name, patch.store, zone, file.getParentDirectory().getSiblingFile("Wave"));
    return true;
}

juce::File VSynthEmuProcessor::lastPatchFolder() const {
    const juce::File f(appSettings()->getValue(kPatchFolderKey));
    return f.isDirectory() ? f : juce::File::getSpecialLocation(juce::File::userHomeDirectory);
}

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
        else notes.add("no wave " + juce::String(number));
    }
    setPatchControls(c);   // and the Multi Step Modulator stopped, as the XT's patch load does (PluginMsm.cpp, v193)
    for (int k = 0; k < 2; ++k)
        if (auto* p = apvts.getParameter(Params::oscId(k, Params::kEncodeType))) p->setValueNotifyingHost(p->convertTo0to1(0.0f));   // Auto: each wave's own
    for (const auto& fx : xtPatchEffects(store)) {
        const char* knob = nullptr;
        for (const auto& k : kFxKnobs) if (k.id == fx.id) knob = k.name;
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

// Zones 2…16 (the user's decision, 2026-11-05): the patch's data, not the parameters.  Their settings go to the synth at once, analog OSCs
// and all, then again with their waves once the pool has encoded them: each wave once, as its own type, its own tuning and loop on it.
void VSynthEmuProcessor::loadZones(const std::vector<uint8_t>& store, const juce::File& waveFolder, juce::StringArray& notes) {
    const int gen = ++zonesGen_;
    const int zones = xtPatchZones(store);
    patchZones_.store(zones);
    apvts.state.setProperty("patchStore", zones > 1 ? juce::Base64::toBase64(store.data(), store.size()) : juce::String(), nullptr);
    apvts.state.setProperty("patchWaves", zones > 1 ? waveFolder.getFullPathName() : juce::String(), nullptr);
    if (zones < 2) { synth_.setZones(nullptr); zonesReady_.store(true); return; }
    notes.add(juce::String(zones) + " zones, zone 1 on the panel");
    auto set = std::make_shared<XtSynth::Zones>();
    xtPatchZoneSet(store, xtControls(), *set);
    struct Want { int zone, osc, number; };
    std::vector<Want> want;
    std::map<int, XtWaveFile> files;
    for (int z = 1; z < zones; ++z)
        for (int k = 0; k < 2; ++k) {
            XtSynth::OscControls& o = k ? set->ctl[z].osc2 : static_cast<XtSynth::OscControls&>(set->ctl[z]);
            const int number = xtPatchWaveNumber(store, z, k);
            if (o.oscType != 1 || number <= 0) continue;
            if (!files.count(number)) {
                XtWaveFile w;
                const juce::File f = findWave(waveFolder, number);
                if (!(f.existsAsFile() && readVSynthWave(f, w))) { w = XtWaveFile{}; notes.addIfNotAlreadyThere("no wave " + juce::String(number)); }
                files[number] = std::move(w);
            }
            const XtWaveFile& w = files[number];
            if (w.frames <= 0) continue;
            xtWaveOntoControls(w, o);
            want.push_back({ z, k, number });
        }
    synth_.setZones(set);
    xtActive_.store(true);
    zonesReady_.store(want.empty());
    if (want.empty()) return;
    addEncode([this, gen, set, want = std::move(want), files = std::move(files)](const std::atomic<bool>&) {
        std::map<int, std::shared_ptr<XtEncoded>> enc;
        for (const auto& [number, w] : files) {
            if (gen != zonesGen_.load()) return;
            if (w.frames <= 0) continue;
            std::vector<std::vector<float>> chans((size_t)w.channels);
            for (long i = 0; i < w.frames; ++i)
                for (int c = 0; c < w.channels; ++c) chans[(size_t)c].push_back((float)w.pcm[(size_t)(i * w.channels + c)] / 32768.0f);
            const int type = w.encodeType == 0 || w.encodeType == 1 || w.encodeType == 3 || w.encodeType == 5 ? w.encodeType : 1;
            auto e = xtSynthEncode(chans, 44100.0, type, 64, nullptr, 16, &w.events, &w.vpdt);   // on its own vpdt, as the file has it
            if (!e) continue;
            xtWaveOntoEncoded(w, *e);
            enc[number] = std::move(e);
        }
        auto withWaves = std::make_shared<XtSynth::Zones>(*set);
        for (const Want& x : want)
            if (const auto it = enc.find(x.number); it != enc.end()) withWaves->wave[x.zone][x.osc] = it->second;
        if (gen != zonesGen_.load()) return;
        synth_.setZones(withWaves);
        zonesReady_.store(true);
    });
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
    engine_.setParams(engineParams());
}

VSE::VariphraseParams VSynthEmuProcessor::engineParams() const {
    VSE::VariphraseParams p;
    p.pitchShiftSemitones   = apvts.getRawParameterValue(Params::kPitchShift)->load();
    p.timeStretchRatio      = apvts.getRawParameterValue(Params::kTimeStretch)->load();
    p.formantShiftSemitones = apvts.getRawParameterValue(Params::kFormantShift)->load();
    p.robotMode             = apvts.getRawParameterValue(Params::kRobot)->load() > 0.5f;
    return p;
}

void VSynthEmuProcessor::setAlgorithm(VSE::Algorithm algo) {
    engine_.setAlgorithm(algo);
}

VSE::Algorithm VSynthEmuProcessor::getAlgorithm() const {
    return engine_.getAlgorithm();
}

// The state, getStateInformation and setStateInformation, is in PluginState.cpp (v172).

// ─── Editor / Entry Point ─────────────────────────────────────────────────────

juce::AudioProcessorEditor* VSynthEmuProcessor::createEditor() {
    return new VSynthEmuEditor(*this);
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
