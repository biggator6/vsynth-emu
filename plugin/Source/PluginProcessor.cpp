#include "PluginProcessor.h"
#include "PluginEditor.h"

// ─── Parameter IDs ───────────────────────────────────────────────────────────
namespace Params {
    static const juce::String kPitchShift   = "pitchShift";
    static const juce::String kTimeStretch  = "timeStretch";
    static const juce::String kFormantShift = "formantShift";
    static const juce::String kRobot        = "robot";
    static const juce::String kEncodeType   = "encodeType";    // Auto, SOLO, BACKING, ENSEMBLE
    static const juce::String kEncodeDepth  = "encodeDepth";   // multi-band attack detection
    static const juce::String kRootKey      = "rootKey";       // note that plays the sample unshifted
    static const juce::String kEnergy       = "energy";        // SOLO Energy g (knob mapping undecoded)
    // Amp envelope, the XT's 0..127 knobs (XtEnvelope: times from the firmware's table)
    static const juce::String kAttack       = "ampAttack";
    static const juce::String kDecay        = "ampDecay";
    static const juce::String kSustain      = "ampSustain";
    static const juce::String kRelease      = "ampRelease";
    static const juce::String kAttackSens   = "ampAttackSens";   // velocity → time, −63..+63
    static const juce::String kDecaySens    = "ampDecaySens";
    static const juce::String kReleaseSens  = "ampReleaseSens";  // note-off velocity
    static const juce::String kTimeKF       = "ampTimeKF";       // −200..+200, decay and release
    // Pitch envelope: 0..127 knobs, depth −63..+63 (±6000 cents at 63)
    static const juce::String kPAttack      = "pitchEnvAttack";
    static const juce::String kPDecay       = "pitchEnvDecay";
    static const juce::String kPSustain     = "pitchEnvSustain";
    static const juce::String kPRelease     = "pitchEnvRelease";
    static const juce::String kPDepth       = "pitchEnvDepth";
}

// ─── Parameter Layout ─────────────────────────────────────────────────────────

juce::AudioProcessorValueTreeState::ParameterLayout
VSynthEmuProcessor::createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        Params::kPitchShift, "Pitch Shift",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.01f),
        0.0f, "st"
    ));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        Params::kTimeStretch, "Time Stretch",
        juce::NormalisableRange<float>(0.25f, 4.0f, 0.001f, 0.4f),
        1.0f, "x"
    ));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        Params::kFormantShift, "Formant Shift",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.01f),
        0.0f, "st"
    ));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        Params::kRobot, "Robot Mode", false
    ));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        Params::kEncodeType, "Encode Type",
        juce::StringArray { "Auto", "SOLO", "BACKING", "ENSEMBLE" }, 0
    ));

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        Params::kEncodeDepth, "Encode Depth", 0, 127, 64
    ));

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        Params::kRootKey, "Original Key", 0, 127, 60
    ));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        Params::kEnergy, "Energy",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
        0.0f
    ));

    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kAttack, "Attack", 0, 127, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kDecay, "Decay", 0, 127, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kSustain, "Sustain", 0, 127, 127));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kRelease, "Release", 0, 127, 16));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kAttackSens, "Attack Velocity Sens", -63, 63, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kDecaySens, "Decay Velocity Sens", -63, 63, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kReleaseSens, "Release Velocity Sens", -63, 63, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        Params::kTimeKF, "Env Time KF", juce::NormalisableRange<float>(-200.0f, 200.0f, 10.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kPAttack, "Pitch Env Attack", 0, 127, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kPDecay, "Pitch Env Decay", 0, 127, 64));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kPSustain, "Pitch Env Sustain", 0, 127, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kPRelease, "Pitch Env Release", 0, 127, 64));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kPDepth, "Pitch Env Depth", -63, 63, 0));

    return { params.begin(), params.end() };
}

// ─── Constructor / Destructor ─────────────────────────────────────────────────

VSynthEmuProcessor::VSynthEmuProcessor()
    // Instrument: stereo output only, no sidechain input
    : AudioProcessor(BusesProperties()
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "VSynthEmu", createParameterLayout())
{
    formatManager_.registerBasicFormats();   // WAV, AIFF, OGG, FLAC

    for (const auto& id : { Params::kPitchShift, Params::kTimeStretch, Params::kFormantShift, Params::kRobot,
                            Params::kEncodeType, Params::kEncodeDepth })
        apvts.addParameterListener(id, this);
    findRom();
}

VSynthEmuProcessor::~VSynthEmuProcessor() {
    for (const auto& id : { Params::kPitchShift, Params::kTimeStretch, Params::kFormantShift, Params::kRobot,
                            Params::kEncodeType, Params::kEncodeDepth })
        apvts.removeParameterListener(id, this);
    encodePool_.removeAllJobs(true, 10000);
}

// ─── The firmware image ───────────────────────────────────────────────────────

// VSE_XT_ROM, else ~/Library/Application Support/V-Synth Emu/V-SynthXT.rom.2.bin
// (rom.2 as research/ghidra_scripts/unpack_firmware.py writes it).
void VSynthEmuProcessor::findRom() {
    juce::StringArray cand;
    if (const char* p = std::getenv("VSE_XT_ROM")) cand.add(p);
    cand.add(juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                 .getChildFile("Application Support/V-Synth Emu/V-SynthXT.rom.2.bin").getFullPathName());
    for (const auto& c : cand)
        if (juce::File(c).existsAsFile() && xtSynthSetRom(c.toStdString())) {
            apvts.state.setProperty("romPath", c, nullptr);
            setStatus("ROM ready");
            return;
        }
    setStatus("No ROM: choose V-SynthXT.rom.2.bin to play through the XT's firmware");
}

bool VSynthEmuProcessor::setRomFile(const juce::File& file) {
    if (!file.existsAsFile() || !xtSynthSetRom(file.getFullPathName().toStdString())) {
        setStatus("Not a usable rom.2 image: " + file.getFileName());
        return false;
    }
    apvts.state.setProperty("romPath", file.getFullPathName(), nullptr);
    setStatus("ROM ready");
    reencode();
    return true;
}

// ─── Lifecycle ────────────────────────────────────────────────────────────────

void VSynthEmuProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    engine_.prepare(sampleRate, samplesPerBlock);
    engine_.setAlgorithm(VSE::Algorithm::Passthrough);
    syncParamsToEngine();
    setLatencySamples(engine_.getLatencySamples());
    synth_.prepare(sampleRate);
    scratch_.assign((size_t)std::max(samplesPerBlock, 1) * 2, 0.0f);
    playbackPos_  = -1;
    activeVoices_ = 0;
}

void VSynthEmuProcessor::releaseResources() {
    engine_.reset();
}

// ─── Process ──────────────────────────────────────────────────────────────────

XtSynth::Controls VSynthEmuProcessor::xtControls() const {
    // Snapped to the parameters' steps: the ranges' float conversions leave
    // the defaults at e.g. 0.99999994, and the firmware's fixed-point steps
    // would play that, not 1 (a centred XT knob is exact).
    auto snap = [](float v, float stepSize) { return std::round(v / stepSize) * stepSize; };
    XtSynth::Controls c;
    c.pitchSemis   = snap(apvts.getRawParameterValue(Params::kPitchShift)->load(), 0.01f);
    c.timeStretch  = snap(apvts.getRawParameterValue(Params::kTimeStretch)->load(), 0.001f);
    c.formantSemis = snap(apvts.getRawParameterValue(Params::kFormantShift)->load(), 0.01f);
    c.energy       = apvts.getRawParameterValue(Params::kEnergy)->load();
    c.rootKey      = (int)apvts.getRawParameterValue(Params::kRootKey)->load();
    c.attack       = (int)apvts.getRawParameterValue(Params::kAttack)->load();
    c.decay        = (int)apvts.getRawParameterValue(Params::kDecay)->load();
    c.sustain      = (int)apvts.getRawParameterValue(Params::kSustain)->load();
    c.release      = (int)apvts.getRawParameterValue(Params::kRelease)->load();
    c.aSens        = (int)apvts.getRawParameterValue(Params::kAttackSens)->load();
    c.dSens        = (int)apvts.getRawParameterValue(Params::kDecaySens)->load();
    c.rSens        = (int)apvts.getRawParameterValue(Params::kReleaseSens)->load();
    c.timeKF       = 10 * (int)std::lround(apvts.getRawParameterValue(Params::kTimeKF)->load() / 10.0f);
    c.pAttack      = (int)apvts.getRawParameterValue(Params::kPAttack)->load();
    c.pDecay       = (int)apvts.getRawParameterValue(Params::kPDecay)->load();
    c.pSustain     = (int)apvts.getRawParameterValue(Params::kPSustain)->load();
    c.pRelease     = (int)apvts.getRawParameterValue(Params::kPRelease)->load();
    c.pDepth       = (int)apvts.getRawParameterValue(Params::kPDepth)->load();
    return c;
}

void VSynthEmuProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                       juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const int numOut     = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    // ── The XT's own players: one voice per note, sample-accurate MIDI ───────
    if (xtActive_.load() && numOut > 0) {
        synth_.setControls(xtControls());
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
            else if (msg.isAllNotesOff() || msg.isAllSoundOff()) synth_.allNotesOff();
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

    if (playbackPos_ < 0 || !fileLoaded_.load())
        return;   // nothing to play — buffer is already silent

    // ── Grab the file buffer (non-blocking) ───────────────────────────────────
    juce::GenericScopedTryLock<juce::CriticalSection> lock(bufferLock_);
    if (!lock.isLocked())
        return;   // file is being swapped out — skip this block silently

    const int fileSamples = fileBuffer_.getNumSamples();
    const int fileChans   = fileBuffer_.getNumChannels();

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
                           fileBuffer_, ch,
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
}

// ─── File Loading ─────────────────────────────────────────────────────────────

void VSynthEmuProcessor::loadFile(const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader(
        formatManager_.createReaderFor(file));

    if (!reader)
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Load Error",
            "Could not read: " + file.getFileName());
        return;
    }

    const int totalSamples = static_cast<int>(reader->lengthInSamples);
    const int numChans     = static_cast<int>(reader->numChannels);

    juce::AudioBuffer<float> newBuffer(numChans, totalSamples);
    reader->read(&newBuffer, 0, totalSamples, 0, true, true);

    {
        juce::CriticalSection::ScopedLockType lock(bufferLock_);
        fileBuffer_ = std::move(newBuffer);
        fileRate_ = reader->sampleRate;
        fileBits_ = reader->usesFloatingPointData ? 16 : (int)reader->bitsPerSample;   // float WAVs: rounded
        fileLoaded_.store(true);
    }

    playbackPos_  = -1;
    activeVoices_ = 0;
    loadedFileName_ = file.getFileName();

    // Persist the file path so it survives DAW project reload
    apvts.state.setProperty("loadedFilePath", file.getFullPathName(), nullptr);
    reencode();
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
void VSynthEmuProcessor::reencode() {
    if (!fileLoaded_.load()) return;
    if (!xtSynthRomReady()) { xtActive_.store(false); return; }
    std::vector<std::vector<float>> chans;
    double rate;
    int bits;
    {
        juce::CriticalSection::ScopedLockType lock(bufferLock_);
        for (int c = 0; c < std::min(2, fileBuffer_.getNumChannels()); ++c)
            chans.emplace_back(fileBuffer_.getReadPointer(c), fileBuffer_.getReadPointer(c) + fileBuffer_.getNumSamples());
        rate = fileRate_;
        bits = fileBits_;
    }
    const int choice = (int)apvts.getRawParameterValue(Params::kEncodeType)->load();
    const int depth  = (int)apvts.getRawParameterValue(Params::kEncodeDepth)->load();
    const int gen = ++encodeGen_;
    setStatus("Encoding...");
    encodePool_.addJob([this, chans = std::move(chans), rate, bits, choice, depth, gen] {
        int type = choice == 1 ? 1 : choice == 2 ? 3 : choice == 3 ? 5 : 0;
        if (type == 0) {   // Auto: the engine's content classifier
            std::vector<float> mono(chans[0].size());
            for (size_t i = 0; i < mono.size(); ++i) {
                float s = 0; for (const auto& c : chans) s += c[i];
                mono[i] = s / (float)chans.size();
            }
            using CT = VSE::VariphraseAnalysis::ContentType;
            const auto a = VSE::VariphraseEngine::analyzeContent(mono.data(), (int)mono.size(), rate);
            type = a.contentType == CT::BACKING ? 3 : a.contentType == CT::ENSEMBLE ? 5 : 1;   // LITE plays on the SOLO player
        }
        std::string err;
        auto e = xtSynthEncode(chans, rate, type, depth, &err, bits);
        if (gen != encodeGen_.load()) return;
        if (!e) { setStatus("Encode failed: " + juce::String(err)); return; }
        synth_.setSample(e);
        xtActive_.store(true);
        static const char* names[] = { "", "SOLO", "", "BACKING", "", "ENSEMBLE" };
        setStatus(juce::String("Encoded as ") + names[type] + (choice == 0 ? " (auto)" : ""));
    });
}

void VSynthEmuProcessor::handleAsyncUpdate() { reencode(); }

// ─── Parameters ───────────────────────────────────────────────────────────────

void VSynthEmuProcessor::parameterChanged(const juce::String& paramID, float /*newValue*/) {
    if (paramID == Params::kEncodeType || paramID == Params::kEncodeDepth) { triggerAsyncUpdate(); return; }
    syncParamsToEngine();
}

void VSynthEmuProcessor::syncParamsToEngine() {
    VSE::VariphraseParams p;
    p.pitchShiftSemitones   = apvts.getRawParameterValue(Params::kPitchShift)->load();
    p.timeStretchRatio      = apvts.getRawParameterValue(Params::kTimeStretch)->load();
    p.formantShiftSemitones = apvts.getRawParameterValue(Params::kFormantShift)->load();
    p.robotMode             = apvts.getRawParameterValue(Params::kRobot)->load() > 0.5f;
    engine_.setParams(p);
}

void VSynthEmuProcessor::setAlgorithm(VSE::Algorithm algo) {
    engine_.setAlgorithm(algo);
}

VSE::Algorithm VSynthEmuProcessor::getAlgorithm() const {
    return engine_.getAlgorithm();
}

// ─── State ────────────────────────────────────────────────────────────────────

void VSynthEmuProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void VSynthEmuProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (!xml || !xml->hasTagName(apvts.state.getType()))
        return;

    apvts.replaceState(juce::ValueTree::fromXml(*xml));

    // The firmware image and the file, if their paths were persisted
    const juce::String rom = apvts.state.getProperty("romPath").toString();
    if (rom.isNotEmpty() && !xtSynthRomReady() && juce::File(rom).existsAsFile())
        setRomFile(juce::File(rom));
    juce::String path = apvts.state.getProperty("loadedFilePath").toString();
    if (path.isNotEmpty())
    {
        juce::File f(path);
        if (f.existsAsFile())
            loadFile(f);
    }
}

// ─── Editor / Entry Point ─────────────────────────────────────────────────────

juce::AudioProcessorEditor* VSynthEmuProcessor::createEditor() {
    return new VSynthEmuEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new VSynthEmuProcessor();
}
