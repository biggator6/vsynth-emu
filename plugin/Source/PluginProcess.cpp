// The processor's processBlock, moved as it stood from PluginProcessor.cpp (v201), which had grown past the kit's
// 300 lines: the XT's players with their sample-accurate MIDI, or without the firmware the loaded file as it is.
#include "PluginProcessor.h"
#include "PluginParamIds.h"
#include "PluginOutputFinite.h"

// ─── Process ──────────────────────────────────────────────────────────────────

void VSynthEmuProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                       juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const OutputFinite finite(buffer);       // every way out: no NaN or infinity reaches the host (v454)
    const int numIn = inputBlock(buffer);   // the outside input, left in the first channels for the render (v207)

    // ── The VC-2 mode: its part plays the block, the editor's messages first (PluginVc2Play.h, v455) ───────
    if (auto vc2 = vc2Play_.block()) {
        takeFromEditor([&vc2](const juce::MidiMessage& m) { vc2.message(m); });
        vc2.process(buffer, midi, numIn);
        return;
    }

    const int numOut     = std::min(buffer.getNumChannels(), getTotalNumOutputChannels());
    const int numSamples = buffer.getNumSamples();

    // ── The XT's own players: one voice per note, sample-accurate MIDI (PluginRender.cpp) ───────
    if ((xtActive_.load() || playsWaveless()) && numOut > 0) {
        synth_.setControls(xtControls());
        synth_.useBusSink(raw(Params::kFxOn) >= 0.5f);
        // The host's transport, for Clock Source HOST (the Standalone has none: the Patch Tempo stands)
        double hostBpm = 0.0, hostPpq = 0.0;
        bool hostMoving = false;
        if (auto* head = getPlayHead())
            if (const auto info = head->getPosition()) {
                hostBpm = info->getBpm().orFallback(0.0);
                if (const auto ppq = info->getPpqPosition()) { hostPpq = *ppq; hostMoving = info->getIsPlaying(); }
            }
        synth_.hostClock(hostBpm, hostMoving, hostPpq);
        const int trip = (int)raw(Params::kTripPad);
        if (trip != tripKnob_) { tripKnob_ = trip; synth_.timeTripKnob(trip); }
        float* L = buffer.getWritePointer(0);
        float* R = numOut > 1 ? buffer.getWritePointer(1) : nullptr;
        if (!R) {
            if ((int)scratch_.size() < numSamples) scratch_.resize((size_t)numSamples);   // normally sized in prepareToPlay
            R = scratch_.data();
        }
        takeFromEditor([this](const juce::MidiMessage& m) { toSynth(m); });   // the editor's, at the block's start (v201)
        playMidi(buffer, midi, numIn, L, R);   // the host's at their frames, the input's spans between (v207)
        if (numOut == 1) for (int i = 0; i < numSamples; ++i) L[i] = 0.5f * (L[i] + R[i]);
        return;
    }

    // ── Without the firmware: the loaded file as it is ──────────────────────────
    playFile(buffer, midi, numOut);
}

// The file played without the firmware, a block of it (moved from processBlock as it stood, v454): its channels mixed to
// mono, at the level the XT's voices play, on every output
void VSynthEmuProcessor::playFile(juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi, int numOut)
{
    const int numSamples = buffer.getNumSamples();
    buffer.clear();   // inputBlock left the input in it (v207)
    takeFromEditor([this](const juce::MidiMessage& m) { toFile(m); });
    for (const auto meta : midi) toFile(meta.getMessage());

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
    // Mix file channels down to mono, then fan back out to every output.
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

    // ── Fan mono output to all output channels ───────────────────────────────
    for (int ch = 0; ch < numOut; ++ch)
        buffer.copyFrom(ch, 0, monoIn, 0, 0, numSamples);
    buffer.applyGain(0.125f);   // at the level the XT's voices play (XtSynth::busOut), so losing the image isn't 18 dB louder
}

// One message to the synth, as the host's MIDI and the editor's reach it (the block's own chain, v201): the mode
// messages are controllers too.  A controller's edits of the patch first, as the XT's OS takes a message before its
// tone generator does (v462: onto the parameters, for the next block's controls)
void VSynthEmuProcessor::toSynth(const juce::MidiMessage& msg) {
    if (msg.isNoteOn()) synth_.noteOn(msg.getNoteNumber(), msg.getFloatVelocity());
    else if (msg.isNoteOff()) synth_.noteOff(msg.getNoteNumber(), msg.getFloatVelocity());
    else if (msg.isPitchWheel()) synth_.pitchBend(msg.getPitchWheelValue());
    else if (msg.isChannelPressure()) synth_.channelPressure(msg.getChannelPressureValue());
    else if (msg.isAftertouch()) synth_.polyPressure(msg.getNoteNumber(), msg.getAfterTouchValue());
    else if (msg.isController()) {
        midi_.controller(msg.getControllerNumber(), msg.getControllerValue());
        synth_.controller(msg.getControllerNumber(), msg.getControllerValue());
    }
}

// One message to the file played without the firmware: a note-on starts it from the top, the last note-off stops it
void VSynthEmuProcessor::toFile(const juce::MidiMessage& msg) {
    if (msg.isNoteOn()) {
        ++activeVoices_;
        playbackPos_ = 0;
    } else if (msg.isNoteOff()) {
        activeVoices_ = std::max(0, activeVoices_ - 1);
        if (activeVoices_ == 0) playbackPos_ = -1;
    } else if (msg.isAllNotesOff() || msg.isAllSoundOff()) {
        activeVoices_ = 0;
        playbackPos_ = -1;
    }
}
