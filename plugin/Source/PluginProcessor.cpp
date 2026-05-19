#include "PluginProcessor.h"
#include "PluginEditor.h"

// ─── Parameter IDs ───────────────────────────────────────────────────────────
namespace Params {
    static const juce::String kPitchShift   = "pitchShift";
    static const juce::String kTimeStretch  = "timeStretch";
    static const juce::String kFormantShift = "formantShift";
    static const juce::String kRobot        = "robot";
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

    apvts.addParameterListener(Params::kPitchShift,   this);
    apvts.addParameterListener(Params::kTimeStretch,  this);
    apvts.addParameterListener(Params::kFormantShift, this);
    apvts.addParameterListener(Params::kRobot,        this);
}

VSynthEmuProcessor::~VSynthEmuProcessor() {
    apvts.removeParameterListener(Params::kPitchShift,   this);
    apvts.removeParameterListener(Params::kTimeStretch,  this);
    apvts.removeParameterListener(Params::kFormantShift, this);
    apvts.removeParameterListener(Params::kRobot,        this);
}

// ─── Lifecycle ────────────────────────────────────────────────────────────────

void VSynthEmuProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    engine_.prepare(sampleRate, samplesPerBlock);
    engine_.setAlgorithm(VSE::Algorithm::Passthrough);
    syncParamsToEngine();
    setLatencySamples(engine_.getLatencySamples());
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

    // ── MIDI: note-on starts playback, note-off decrements voice count ────────
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

    // TODO: resample to getSampleRate() if reader->sampleRate differs.
    // For now, files are assumed to be at 44.1 kHz. The VariphraseEngine's
    // time-stretch handles the perceptual impact of small rate mismatches
    // during batch testing.

    {
        juce::CriticalSection::ScopedLockType lock(bufferLock_);
        fileBuffer_ = std::move(newBuffer);
        fileLoaded_.store(true);
    }

    playbackPos_  = -1;
    activeVoices_ = 0;
    loadedFileName_ = file.getFileName();

    // Persist the file path so it survives DAW project reload
    apvts.state.setProperty("loadedFilePath", file.getFullPathName(), nullptr);
}

// ─── Parameters ───────────────────────────────────────────────────────────────

void VSynthEmuProcessor::parameterChanged(const juce::String& /*paramID*/, float /*newValue*/) {
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

    // Reload the file if a path was persisted
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
