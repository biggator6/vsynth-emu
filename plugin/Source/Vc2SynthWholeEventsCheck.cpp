// xt_vc2synth's whole part's events (Vc2SynthWholeCheck.h; moved out of Vc2SynthWholeCheck.cpp as they stood, v360):
// each drawn as the port's model takes it, a burst's or a flood's inside a wait, the machine given what the model
// took, a wait cut, a step. v360, each by the pair's generator of its own (`own`), so no other draw moves:
// - a fresh pair's lead-in, before its first patch: silent blocks, one event in 16 a channel message on MIDI IN, until
//   the kernel's time passes the sensing check's first 512 ticks (rom.4's 0x89311330 on the receive object it
//   watches: MIDI IN's until the first parameters' message moves it to the second USB port's, which takes no bytes);
// - a case on a pitch: its blocks' frames, where not silent, a saw of `period` frames as loud as the noise's shift, so
//   the pitch tracker's period falls below 200 frames, where the vocoder's glue tells it from another (the noise's
//   periods are 195…630, and the glue clamps every period past 200.45 alike);
// - at MIDI IN's full ring (its marker 2 just written, 0x89312576), the machine given what the model took and both
//   compared there, inside the wait, as a cut does: until the next status byte the parser's count holds what the full
//   ring left it. XT_VC2SYNTH_MAIN only.
#ifdef XT_VC2SYNTH_MAIN
#include "Vc2SynthWholeCheck.h"
#include <algorithm>

namespace {
constexpr uint32_t kBlockFrames = 32, kFramesPerMs = 44100, kMsPerSecond = 1000, kInputShifts = 12;
// The check's own bounds, no firmware's: a wait inside a pass cut at so many events; a burst's messages (more than the
// notes' queue's 127) and a flood's (more than MIDI IN's receive ring's 2047), drawn on MIDI IN or USB
constexpr size_t kLongestWait = 6000;
constexpr uint32_t kBurstFrom = 130, kBursts = 300, kFloodFrom = 2100, kFloods = 300, kBurstChance = 4;
constexpr uint8_t kChannelFirst = 0x80, kSystemFirst = 0xf0, kMidiIn = 1, kInputs = 2;
constexpr int kTries = 64;
// v360: the lead-in's messages one event in 16; a frame's words; the full ring's marker [FW] (its status, its code)
constexpr uint32_t kLeadMessageOneIn = 16, kFrameWords = 8;
constexpr uint8_t kMarkerStatus = 0xfd, kFullCode = 2;
constexpr int64_t kSawTop = 0x7fffffff;

// A channel message as the notes part draws them, from `gen`, drawn again until it is one
Vc2PartEvent channelDrawn(std::mt19937& gen, const Vc2Synth& s) {
    Vc2PartEvent e;
    e.block = false;
    for (int k = 0; k < kTries && (e.message.status < kChannelFirst || e.message.status >= kSystemFirst); ++k) {
        const Vc2SynthDrawn m = vc2SynthMessageDrawn(gen, s);
        e.message = m.m;
        e.word = m.word;
    }
    return e;
}
} // namespace

// A burst's next: a channel message as the notes part draws them, on MIDI IN or USB (a flood's on MIDI IN)
Vc2PartEvent Vc2SynthWhole::burstMessage() {
    Vc2PartEvent e = channelDrawn(rng, s);
    e.message.source = (uint8_t)(flooding ? kMidiIn : kMidiIn + rng() % kInputs);
    --burst;
    ++n.messages;
    return e;
}

// A block two times in three, its frames silence one time in four, else noise shifted down 0…11 bits (v360: a pitch on
// a pitch's case); else a message; a burst's while one goes on (v360: the lead-in's, by `own`, while it goes on)
Vc2PartEvent Vc2SynthWhole::drawn() {
    if (leading) return leadDrawn();
    if (burst > 0) return burstMessage();
    Vc2PartEvent e;
    if (rng() % 3 != 0) {
        const bool silent = rng() % 4 == 0;
        const uint32_t shift = rng() % kInputShifts;
        Vc2SynthInput& frames = inputs.emplace_back();
        for (int32_t& word : frames) word = silent ? 0 : (int32_t)rng() >> shift;
        if (pitched && !silent) pitchOver(frames, shift);
        e.input = frames.data();
        ++n.blocks;
    } else {
        const Vc2SynthDrawn m = vc2SynthMessageDrawn(rng, s);
        e.block = false;
        e.message = m.m;
        e.word = m.word;
        ++n.messages;
    }
    return e;
}

// A wait's first event drawn: one time in four a burst, the pair's first a flood (none in the lead-in)
void Vc2SynthWhole::burstDrawn() {
    if (leading || rng() % kBurstChance != 0) return;
    flooding = !flooded;
    flooded = true;
    burst = flooding ? kFloodFrom + rng() % kFloods : kBurstFrom + rng() % kBursts;
    ++(flooding ? n.floods : n.bursts);
}

// The next event the port's model takes, drawn when none waits; none past the longest wait, cut there; v360: the
// compare at a full ring first
bool Vc2SynthWhole::next(Vc2PartEvent& e) {
    if (taken - stepFrom >= kLongestWait) {
        if (!cutting) waitCut();
        return false;
    }
    fullCompare();
    if (taken == events.size()) {
        if (taken == stepFrom + 1 && burst == 0) burstDrawn();
        events.push_back(drawn());
    }
    e = events[taken++];
    return true;
}

std::string Vc2SynthWhole::give(const Vc2PartEvent& e) {
    if (!e.block) return rig.message(e.message, e.word);
    std::string d;
    for (msAcc += kBlockFrames * kMsPerSecond; msAcc >= kFramesPerMs && d.empty(); msAcc -= kFramesPerMs) {
        ++n.ticks;
        d = rig.tick();
    }
    if (d.empty()) d = rig.input(e.input);
    return d.empty() ? rig.output() : d;
}

// The machine given what the port's model took, both compared there, inside the wait, before the port's unwinds
void Vc2SynthWhole::waitCut() {
    while (given < taken && cut.empty()) cut = give(events[given++]);
    if (cut.empty()) cut = vc2SynthWholeDifference(rig, s);
    if (!cut.empty()) cut = "inside a wait cut: " + cut;
    cutting = true;
    ++n.cuts;
}

// One event or more on the port's model (a wait inside a pass takes the next), then each on the machine
std::string Vc2SynthWhole::step() {
    stepFrom = taken;
    s.osTasks->deliverNext();
    n.nested += taken - stepFrom > 1 ? 1 : 0;
    if (!atFull.empty()) return atFull;
    if (cutting) return cut;
    std::string d;
    while (given < taken && d.empty()) d = give(events[given++]);
    n.sounding += std::ranges::any_of(s.frames, [](float x) { return x != 0.0f; }) ? 1 : 0;
    return d.empty() ? vc2SynthWholeDifference(rig, s) : d;
}

// v360: where the port's MIDI IN ring's last write is a full ring's marker not looked at before, the machine given
// what the model took and both compared, inside the wait
void Vc2SynthWhole::fullCompare() {
    const Vc2MidiReceive& r = s.midiPath.midiIn;
    if (r.w.write == fullWrite || r.w.write < r.w.start || r.w.write >= r.w.end) return;
    fullWrite = r.w.write;
    const auto* marker = reinterpret_cast<const uint8_t*>(&r.ring[(r.w.write - r.w.start) / 4]);
    if (marker[0] != kMarkerStatus || marker[1] != kFullCode || cutting || !atFull.empty()) return;
    ++n.fulls;
    while (given < taken && atFull.empty()) atFull = give(events[given++]);
    if (atFull.empty()) atFull = vc2SynthWholeDifference(rig, s);
    if (!atFull.empty()) atFull = "at MIDI IN's full ring, inside a wait: " + atFull;
}

// v360: the lead-in's next, by `own`: a silent block, or one time in 16 a channel message on MIDI IN
Vc2PartEvent Vc2SynthWhole::leadDrawn() {
    ++n.leads;
    if (own() % kLeadMessageOneIn == 0) {
        Vc2PartEvent e = channelDrawn(own, s);
        e.message.source = kMidiIn;
        return e;
    }
    Vc2PartEvent e;
    Vc2SynthInput& frames = inputs.emplace_back();
    frames.fill(0);
    e.input = frames.data();
    return e;
}

// v360: the lead-in's steps until the kernel's time passes `ticks`; what differed in it
std::string Vc2SynthWhole::leadIn(uint32_t ticks) {
    leading = true;
    std::string d;
    while (d.empty() && !cutting && s.osTasks->os.kernel.time < ticks) d = step();
    leading = false;
    return d.empty() ? "" : "the lead-in: " + d;
}

// v360: a saw of `period` frames over every word of each frame, its phase on from the last block's, shifted as the
// noise is
void Vc2SynthWhole::pitchOver(Vc2SynthInput& frames, uint32_t shift) {
    for (size_t f = 0; f < frames.size() / kFrameWords; ++f) {
        const int64_t ramp = (int64_t)phase * kSawTop / period;
        const auto v = (int32_t)((ramp * 2 - kSawTop) >> shift);
        for (size_t w = 0; w < kFrameWords; ++w) frames[kFrameWords * f + w] = v;
        phase = (phase + 1) % period;
    }
}
#endif
