// The VC-2 part's MIDI dispatcher: see Vc2MidiDispatch.h (v308).  Each class's way a row, in rom.4's order of tests;
// each call's arguments as rom.4 gives them.
#include "Vc2MidiDispatch.h"
#include <algorithm>
#include <iterator>

namespace {
using H = Vc2MidiHandler;
constexpr uint8_t kChannelFirst = 0x80, kSystemFirst = 0xf0, kClassBits = 0xf0, kChannelBits = 0x0f;   // [FW]
constexpr uint32_t kOffVelocity = 64;                                    // [FW] a note-on of velocity 0's
constexpr uint32_t kSystemKind = 3, kKindBits = 0xff;                    // [FW] a parameters' word's kind
constexpr int16_t kDcCut = 0x532;                                        // [FW] Out DC cut for TG
constexpr uint32_t kSoundOff = 120, kResetAll = 121, kNotesOff = 123;    // [FW] the reset's controllers
constexpr int32_t kChannels = 16;

// A message's call: the world, its source, channel and data, and the word
struct MidiCall {
    Vc2MidiWorld& w;
    int32_t source, channel;
    uint32_t data1, data2, word;
};
using MidiWay = void (*)(const MidiCall&, H);

// A channel message to its handler
void handled(const MidiCall& c, H h) { c.w.parts.channelMessage(h, c.source, c.channel, c.data1, c.data2); }

// A velocity of 0 the note-off at 64; else the note-on with the gate clear
void noteOn(const MidiCall& c, H h) {
    if (c.data2 == 0) {
        c.w.parts.channelMessage(H::kNoteOff, c.source, c.channel, c.data1, kOffVelocity);
        return;
    }
    if (c.w.noteGate == 0) handled(c, h);
}

// A parameters' word of the system's kind: Out DC cut for TG read and given on first
void dcCutFirst(const MidiCall& c) {
    if ((c.word & kKindBits) != kSystemKind) return;
    c.w.parts.dcCut(c.w.params.value(Vc2ParamBlock::kSystemC, 0, 0, kDcCut));
}

void parameters(const MidiCall& c, H) {
    dcCutFirst(c);
    c.w.parts.parameters(c.word);
}

void partParameters(const MidiCall& c, H) {
    dcCutFirst(c);
    c.w.parts.partParameters(c.word);
}

// 0x89390340: with a note held, Reset All Controllers on each channel; else All Sound Off, All Notes Off and Reset All
// Controllers on each, then all notes off
void reset(const MidiCall& c, H) {
    Vc2MidiParts& p = c.w.parts;
    const bool held = c.w.rows.heldNote != 0;
    for (int32_t channel = 0; channel < kChannels; ++channel) {
        if (!held) p.channelMessage(H::kController, c.source, channel, kSoundOff, 0);
        if (!held) p.channelMessage(H::kController, c.source, channel, kNotesOff, 0);
        p.channelMessage(H::kController, c.source, channel, kResetAll, 0);
    }
    if (!held) p.allNotesOff();
}

// A class's way and the handler it is given
struct MidiRow {
    uint8_t cls;
    MidiWay way;
    H handler;
};
// [FW] the classes in rom.4's order of tests
constexpr MidiRow kWays[] = {
    { 0x80, handled, H::kNoteOff },  { 0x90, noteOn, H::kNoteOn },         { 0xa0, handled, H::kPolyPressure },
    { 0xb0, handled, H::kController }, { 0xc0, handled, H::kProgram },     { 0xd0, handled, H::kPressure },
    { 0xe0, handled, H::kBend },       { 0x40, parameters, H::kNoteOff },  { 0x41, partParameters, H::kNoteOff },
    { 0x70, reset, H::kNoteOff },
};
} // namespace

uint8_t vc2MidiClass(uint8_t status) {
    return status >= kChannelFirst && status < kSystemFirst ? static_cast<uint8_t>(status & kClassBits) : status;
}

void vc2MidiDispatch(Vc2MidiWorld& w, const Vc2MidiMessage& m, uint32_t word) {
    const uint8_t cls = vc2MidiClass(m.status);
    const auto* row = std::find_if(std::begin(kWays), std::end(kWays), [&](const MidiRow& r) { return r.cls == cls; });
    if (row == std::end(kWays)) return;
    const MidiCall c { w, m.source, m.status & kChannelBits, m.data1, m.data2, word };
    row->way(c, row->handler);
}
