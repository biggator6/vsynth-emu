// xt_vc2midi's draws and events (v313): the handler on a drawn message, the control and program changes on a port, a
// channel state's routines and the module's own, each on both sides; what differs after.  XT_VC2MIDI_MAIN only.
#ifdef XT_VC2MIDI_MAIN
#include "Vc2OsMidiCheck.h"

namespace {
using M = Vc2OsMidi;
using S = Vc2ChannelState;
constexpr uint32_t kHandlerAt = 0x89314e90;   // [FW] the OS task's handler

// A drawn argument: none, a data byte, a controller's number (aimed at the ones with a way or a default), a channel
// (now and then any byte), a source, the message word's address (its word drawn), an initializer's this, a bend's
// two bytes (half the time the centre's: 0 and 64)
enum class Arg : uint8_t { kNone, kByte, kCc, kChannel, kSource, kWord, kStatesThis, kPortsThis, kBendLsb, kBendMsb };
constexpr uint8_t kCentreLsb = 0, kCentreMsb = 64;

constexpr uint8_t kAimedCcs[] = { 0, 32, 6, 38, 96, 97, 98, 99, 100, 101, 121, 7, 10, 11, 91, 93, 64, 69, 120, 127 };

uint8_t anyCc(std::mt19937& rng) { return rng() % 2 ? kAimedCcs[rng() % std::size(kAimedCcs)] : rng() % 128; }

// A word as the 0x40 messages give it: a kind 0…7 (0, 4 and 5 most), byte 1's bits 6 and 7 half the time
uint32_t anyWord(std::mt19937& rng) {
    const uint32_t kinds[6] = { 0, 4, 5, 0, rng() % 8, (uint32_t)rng() & 0xff };
    const uint32_t byte1 = (rng() % 2 ? 0x80u : 0) | (rng() % 2 ? 0x40u : 0) | (rng() % 0x40);
    return kinds[rng() % 6] | byte1 << 8 | ((uint32_t)rng() & 0xffff0000u);
}

uint32_t drawArg(MidiSides& s, Arg arg, std::mt19937& rng) {
    switch (arg) {
    case Arg::kByte: return rng() % 4 ? rng() % 128 : (rng() % 2 ? 0 : 127);
    case Arg::kCc: return anyCc(rng);
    case Arg::kChannel: return rng() % 8 ? rng() % 16 : rng() % 256;
    case Arg::kSource: return midiAnySource(rng);
    case Arg::kWord: s.fw.cpu.wr32(kMidiWordAt, anyWord(rng)); return kMidiWordAt;
    case Arg::kStatesThis: return kMidiStatesThis;
    case Arg::kPortsThis: return kMidiPortsThis;
    case Arg::kBendLsb: return rng() % 2 ? kCentreLsb : rng() % 128;
    case Arg::kBendMsb: return rng() % 2 ? kCentreMsb : rng() % 128;
    case Arg::kNone: break;
    }
    return 0;
}

// [FW] a channel state's routine: on a drawn state, its two arguments drawn, the port's routine given them
struct StateRow {
    uint32_t routine;
    const char* name;
    Arg a, b;
    void (*port)(S&, uint32_t, uint32_t);
};
constexpr StateRow kStateRows[] = {
    { 0x89317a10, "a state's set-up", Arg::kChannel, Arg::kNone,
      [](S& s, uint32_t a, uint32_t) { vc2ChannelSetUp(s, (uint8_t)a); } },
    { 0x89317a50, "a state's full reset", Arg::kNone, Arg::kNone,
      [](S& s, uint32_t, uint32_t) { vc2ChannelFullReset(s); } },
    { 0x89317a90, "Reset All Controllers", Arg::kNone, Arg::kNone,
      [](S& s, uint32_t, uint32_t) { vc2ChannelResetControllers(s); } },
    { 0x89317b30, "a controller's default", Arg::kCc, Arg::kNone,
      [](S& s, uint32_t a, uint32_t) { vc2ChannelDefault(s, (int)a); } },
    { 0x89317c20, "a controller's store", Arg::kCc, Arg::kByte,
      [](S& s, uint32_t a, uint32_t b) { vc2ChannelStore(s, (int)a, (uint8_t)b); } },
    { 0x89317c60, "a bend", Arg::kBendLsb, Arg::kBendMsb,
      [](S& s, uint32_t a, uint32_t b) { vc2ChannelBend(s, (uint8_t)a, (uint8_t)b); } },
    { 0x89317cc0, "a poly pressure", Arg::kByte, Arg::kByte,
      [](S& s, uint32_t a, uint32_t b) { vc2ChannelPoly(s, (uint8_t)a, (uint8_t)b); } },
    { 0x89317cf0, "a channel pressure", Arg::kByte, Arg::kNone,
      [](S& s, uint32_t a, uint32_t) { vc2ChannelPressure(s, (uint8_t)a); } },
};

template <size_t I>
std::string stateEvent(MidiSides& s, std::mt19937& rng) {
    const StateRow& row = kStateRows[I];
    const uint32_t at = midiAnyState(rng);
    midiSteerState(s, at, rng);
    const uint32_t a = drawArg(s, row.a, rng), b = drawArg(s, row.b, rng);
    ++s.n.byEvent[3 + I];
    return midiOnBoth(s, { row.routine, row.name }, { at, a, b },
                      [&](Vc2OsMidiParts&, Vc2PartParams&) { row.port(s.port.state(at), a, b); });
}

// [FW] the module's own routines: one argument drawn, the port's routine given it (the word's, the word itself)
struct OwnRow {
    uint32_t routine;
    const char* name;
    Arg a;
    void (*port)(M&, Vc2PartParams&, uint32_t);
};
constexpr OwnRow kOwnRows[] = {
    { 0x89317780, "a source's reset", Arg::kSource,
      [](M& m, Vc2PartParams&, uint32_t a) { m.resetSource((uint8_t)a); } },
    { 0x893176d0, "a channel's reset", Arg::kChannel,
      [](M& m, Vc2PartParams&, uint32_t a) { m.resetChannel((int)a); } },
    { 0x893178a0, "every channel's reset", Arg::kWord, [](M& m, Vc2PartParams&, uint32_t a) { m.resetAll(a); } },
    { 0x893178d0, "the Receive flags' read", Arg::kNone, [](M& m, Vc2PartParams& p, uint32_t) { m.readFlags(p); } },
    { 0x89317d00, "the states' initializer", Arg::kStatesThis,
      [](M& m, Vc2PartParams&, uint32_t) { m.staticInitStates(); } },
    { 0x89317910, "the port objects' initializer", Arg::kPortsThis,
      [](M& m, Vc2PartParams&, uint32_t) { m.staticInitPorts(); } },
};

template <size_t I>
std::string ownEvent(MidiSides& s, std::mt19937& rng) {
    const OwnRow& row = kOwnRows[I];
    uint32_t a = drawArg(s, row.a, rng);
    if (row.a == Arg::kChannel) a %= 16;   // a channel's reset indexes its rows by it
    const uint32_t mine = row.a == Arg::kWord ? s.fw.cpu.rd32(a) : a;
    ++s.n.byEvent[11 + I];
    return midiOnBoth(s, { row.routine, row.name }, { a },
                      [&](Vc2OsMidiParts&, Vc2PartParams& p) { row.port(s.port, p, mine); });
}

// A message of every class: the channel messages on a drawn channel (the controllers aimed), notes, the 0x40 and 0x41
// messages, 0x01, 0x70, a system and a real-time status, any byte; its source and word drawn
std::string handleEvent(MidiSides& s, std::mt19937& rng) {
    const uint8_t classes[14] = { 0xa0, 0xb0, 0xb0, 0xc0, 0xd0, 0xe0, 0x80, 0x90, 0x40, 0x41, 0x01, 0x70, 0xf0, 0xf8 };
    const uint8_t pick = (uint8_t)(rng() % 15);
    uint8_t status = pick < 14 ? classes[pick] : (uint8_t)rng();
    if (status >= 0x80 && status < 0xf0) status = (uint8_t)(status | rng() % 16);
    const bool control = (status & 0xf0) == 0xb0, centred = (status & 0xf0) == 0xe0 && rng() % 4 == 0;
    const auto d1 = (uint8_t)(centred ? kCentreLsb : control ? anyCc(rng) : rng() % 128);
    const auto d2 = (uint8_t)(centred ? kCentreMsb : rng() % 4 ? rng() % 128 : 0);
    const Vc2MidiMessage m { status, d1, d2, midiAnySource(rng) };
    const uint32_t word = anyWord(rng);
    if (status >= 0x80 && status < 0xf0) {
        const uint32_t at = s.port.portOf(m.source, status & 15);
        midiSteerPort(s, at, rng);
        midiSteerState(s, s.port.port(at).state, rng);
    }
    s.fw.cpu.write(kMidiMessageAt, &m, sizeof m);
    s.fw.cpu.wr32(kMidiWordAt, word);
    ++s.n.byEvent[0];
    s.n.byStatus += status < 0xf8 ? 1 : 0;
    return midiOnBoth(s, { kHandlerAt, "the handler" }, { 0, kMidiMessageAt, kMidiWordAt },
                      [&](Vc2OsMidiParts& parts, Vc2PartParams& p) { s.port.handle(m, word, parts, p); });
}

std::string controlEvent(MidiSides& s, std::mt19937& rng) {
    const uint32_t at = midiAnyPort(rng);
    midiSteerPort(s, at, rng);
    const uint8_t cc = anyCc(rng), value = (uint8_t)(rng() % 4 ? rng() % 128 : 0);
    ++s.n.byEvent[1];
    return midiOnBoth(s, { 0x893172e0, "a control change" }, { at, cc, value },
                      [&](Vc2OsMidiParts& parts, Vc2PartParams&) { s.port.controlChange(at, cc, value, parts); });
}

std::string programEvent(MidiSides& s, std::mt19937& rng) {
    const uint32_t at = midiAnyPort(rng);
    midiSteerPort(s, at, rng);
    const auto program = (uint8_t)(rng() % 128);
    ++s.n.byEvent[2];
    return midiOnBoth(s, { 0x893172a0, "a program change" }, { at, program },
                      [&](Vc2OsMidiParts& parts, Vc2PartParams&) { s.port.programChange(at, program, parts); });
}

constexpr MidiEvent kEvents[] = {
    handleEvent, handleEvent, handleEvent, handleEvent, controlEvent, controlEvent, programEvent,
    stateEvent<0>, stateEvent<1>, stateEvent<2>, stateEvent<3>, stateEvent<4>, stateEvent<5>, stateEvent<6>,
    stateEvent<7>, ownEvent<0>, ownEvent<1>, ownEvent<2>, ownEvent<3>, ownEvent<4>, ownEvent<5>,
};

// A port's listeners: none half the time, else a chain of one to three nodes in address order (no cycle), each on
// one of the objects
uint32_t drawListeners(MidiSides& s, std::mt19937& rng) {
    if (rng() % 2) return 0;
    uint32_t next = 0;
    for (int k = kMidiNodes - 1, left = 1 + (int)(rng() % 3); k >= 0 && left > 0; --k) {
        if (rng() % 2) continue;
        const uint32_t node = kMidiNodesAt + 16 * (uint32_t)k;
        s.fw.cpu.wr32(node, kMidiObjectsAt + 16 * (rng() % kMidiObjects));
        s.fw.cpu.wr32(node + 0xc, next);
        next = node;
        --left;
    }
    return next;
}

uint16_t anyNumber(std::mt19937& rng) {
    const uint16_t numbers[5] = { M::kNone, (uint16_t)(rng() % 0x80), (uint16_t)(0x8000 | rng() % 0x8000),
                                  (uint16_t)rng(), (uint16_t)(rng() % 0x80 << 8 | rng() % 0x80) };
    return numbers[rng() % 5];
}

uint8_t anyFlag(std::mt19937& rng) { return rng() % 3 ? (uint8_t)(rng() % 2) : (uint8_t)rng(); }
} // namespace

std::span<const MidiEvent> midiEvents() { return kEvents; }

void midiSteerPort(MidiSides& s, uint32_t at, std::mt19937& rng) {
    if (rng() % 2) return;
    Vc2MidiPort& p = s.port.port(at);
    if (rng() % 2) p.state = midiAnyState(rng);
    p.rpn = anyNumber(rng);
    p.nrpn = anyNumber(rng);
    p.data = rng() % 2 ? (uint16_t)rng() : p.data;
    p.listeners = drawListeners(s, rng);
}

void midiSteerState(MidiSides& s, uint32_t at, std::mt19937& rng) {
    if (rng() % 2) return;
    S& st = s.port.state(at);
    for (const int byte : { S::kBendUp, S::kBendDown, S::kLastPressure })
        st.controllers[(size_t)byte] = (uint8_t)(rng() % 2 ? rng() % 128 : 0);
}

uint32_t midiAnyState(std::mt19937& rng) { return M::kStatesAt + sizeof(S) * (rng() % M::kStates); }
uint32_t midiAnyPort(std::mt19937& rng) { return M::kPortsAt + sizeof(Vc2MidiPort) * (rng() % M::kPorts); }
uint8_t midiAnySource(std::mt19937& rng) { return (uint8_t)(rng() % 5); }

void midiDraw(MidiSides& s, std::mt19937& rng) {
    if (rng() % 16 == 0)
        for (uint32_t& d : s.deltas) d = rng() % 2 ? 0 : 4 * (rng() % 3);
    for (size_t k = 0; k < std::size(kMidiSlots); ++k) {
        s.fw.cpu.wr32(kMidiVtableAt + kMidiSlots[k], s.deltas[k]);
        s.fw.cpu.wr32(kMidiVtableAt + kMidiSlots[k] + 8, kMidiSlotsAt + kMidiSlots[k]);
    }
    for (int k = 0; k < kMidiObjects; ++k) s.fw.cpu.wr32(kMidiObjectsAt + 16 * (uint32_t)k + 4, kMidiVtableAt);
    if (rng() % 8 == 0) {
        S& st = s.port.states[rng() % M::kStates];
        st.bend = (uint16_t)rng();
        st.poly[rng() % 128] = (uint8_t)rng();
        st.controllers[rng() % 100] = (uint8_t)rng();
    }
    if (rng() % 4 == 0) s.port.states[rng() % M::kStates].controllers[S::kStored] = (uint8_t)rng();
    if (rng() % 3 == 0) midiSteerPort(s, midiAnyPort(rng), rng);
    if (rng() % 8 == 0) s.port.receiveProgram = anyFlag(rng);
    if (rng() % 8 == 0) s.port.receiveBank = anyFlag(rng);
    if (rng() % 8 == 0) s.fw.cpu.wr8(kMidiSysexAt, anyFlag(rng));
    for (int32_t& a : s.answers) a = rng() % 3 ? (int32_t)(rng() % 2) : (int32_t)rng();
}
#endif
