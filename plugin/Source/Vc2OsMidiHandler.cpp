// rom.4's OS MIDI task's handler by status, and its control change: see Vc2OsMidi.h (v313).  Each routine's reads,
// calls and writes in rom.4's order.
#include "Vc2OsMidi.h"
#include <algorithm>
#include <iterator>

namespace {
using M = Vc2OsMidi;
using Parts = Vc2OsMidiParts;
constexpr uint16_t kUnselected = 0x8000;            // [FW] a number's bit when the other kind was chosen
constexpr uint16_t kLsbKept = 0x7f80, kMsbKept = 0x00ff, kDataKept = 0x3f80;   // [FW] what a half's write keeps

// A control change's way, by its number (0x893172e0); the rest stored and told
using CcWay = void (*)(M&, Vc2MidiPort&, uint8_t cc, uint8_t value, Parts&);
struct CcRow {
    int cc;
    CcWay way;
};

void ccStore(M& m, Vc2MidiPort& p, uint8_t cc, uint8_t value, Parts& parts) { m.storeAndTell(p, cc, value, parts); }

// One of a number's halves: the value at the half `Keep` keeps, shifted by `Shift`; the other kind unselected
template <uint16_t Vc2MidiPort::*Number, uint16_t Vc2MidiPort::*Other, uint16_t Keep, int Shift>
void ccNumber(M& m, Vc2MidiPort& p, uint8_t cc, uint8_t value, Parts& parts) {
    p.*Number = (uint16_t)((p.*Number & Keep) | value << Shift);
    p.*Other = (uint16_t)(p.*Other | kUnselected);
    m.storeAndTell(p, cc, value, parts);
}

// Bank Select's two numbers, dropped while Receive Bank Select is off
void ccBank(M& m, Vc2MidiPort& p, uint8_t cc, uint8_t value, Parts& parts) {
    if (m.receiveBank != 0) m.storeAndTell(p, cc, value, parts);
}

// Data Entry's two halves (6 the MSB, 38 the LSB), each told to the listeners first
template <uint16_t Keep, int Shift>
void ccData(M& m, Vc2MidiPort& p, uint8_t cc, uint8_t value, Parts& parts) {
    p.data = (uint16_t)((p.data & Keep) | value << Shift);
    m.tellData(p, parts);
    m.storeAndTell(p, cc, value, parts);
}

// Reset All Controllers at 0
void ccReset(M& m, Vc2MidiPort& p, uint8_t cc, uint8_t value, Parts& parts) {
    if (value == 0) {
        vc2ChannelResetControllers(m.state(p.state));
        m.nulled(p);
    }
    m.storeAndTell(p, cc, value, parts);
}

// [FW] the numbers with a way of their own, in rom.4's order of tests
constexpr CcRow kCcWays[] = {
    { 0, ccBank },
    { 32, ccBank },
    { 6, ccData<0, 7> },
    { 38, ccData<kDataKept, 0> },
    { 98, ccNumber<&Vc2MidiPort::nrpn, &Vc2MidiPort::rpn, kLsbKept, 0> },
    { 99, ccNumber<&Vc2MidiPort::nrpn, &Vc2MidiPort::rpn, kMsbKept, 8> },
    { 100, ccNumber<&Vc2MidiPort::rpn, &Vc2MidiPort::nrpn, kLsbKept, 0> },
    { 101, ccNumber<&Vc2MidiPort::rpn, &Vc2MidiPort::nrpn, kMsbKept, 8> },
    { 121, ccReset },
};

// A message's way by its status's class (0x89314e90); the channel messages on the source's and channel's port
using Way = void (*)(M&, const Vc2MidiMessage&, uint32_t word, Parts&, Vc2PartParams&);
struct Row {
    uint8_t status;
    Way way;
};

Vc2ChannelState& stateFor(M& m, const Vc2MidiMessage& msg) {
    return m.state(m.port(m.portOf(msg.source, msg.status & 15)).state);
}

void poly(M& m, const Vc2MidiMessage& msg, uint32_t, Parts&, Vc2PartParams&) {
    vc2ChannelPoly(stateFor(m, msg), msg.data1, msg.data2);   // 0x89317510
}

void control(M& m, const Vc2MidiMessage& msg, uint32_t, Parts& parts, Vc2PartParams&) {
    m.controlChange(m.portOf(msg.source, msg.status & 15), msg.data1, msg.data2, parts);
}

void bend(M& m, const Vc2MidiMessage& msg, uint32_t, Parts&, Vc2PartParams&) {
    vc2ChannelBend(stateFor(m, msg), msg.data1, msg.data2);   // 0x89317500
}

void program(M& m, const Vc2MidiMessage& msg, uint32_t, Parts& parts, Vc2PartParams&) {
    m.programChange(m.portOf(msg.source, msg.status & 15), msg.data1, parts);
}

void pressure(M& m, const Vc2MidiMessage& msg, uint32_t, Parts&, Vc2PartParams&) {
    vc2ChannelPressure(stateFor(m, msg), msg.data1);   // 0x89317520
}

// [FW] a 0x40 message's routines by its word's kind (0…5; 1…3 run none): the OS's, and the two ported here
constexpr uint32_t kResetAllAt = 0x893178a0, kReadFlagsAt = 0x893178d0;
constexpr uint32_t kKindRoutines[6][5] = {
    { 0x89317fe0, kResetAllAt, 0x89318420, 0x89318210 },
    {},
    {},
    {},
    { 0x89318040, kReadFlagsAt, 0x89318450, 0x893154a0, 0x8931a420 },
    { 0x8931a460 },
};

void parameters(M& m, const Vc2MidiMessage&, uint32_t word, Parts& parts, Vc2PartParams& params) {
    const uint32_t kind = word & 0xff;
    if (kind >= std::size(kKindRoutines)) return;
    for (const uint32_t routine : kKindRoutines[kind]) {
        if (routine == kResetAllAt) m.resetAll(word);
        else if (routine == kReadFlagsAt) m.readFlags(params);
        else if (routine != 0) parts.osRoutine(routine, word);
    }
}

void system(M&, const Vc2MidiMessage&, uint32_t word, Parts& parts, Vc2PartParams&) {
    constexpr uint32_t kSysexAt = 0x89310640;   // [FW] given 0x8995dd0c and the word
    if (parts.sysexOn() != 0) parts.osRoutine(kSysexAt, word);
}

void reset(M& m, const Vc2MidiMessage& msg, uint32_t, Parts&, Vc2PartParams&) { m.resetSource(msg.source); }

// [FW] the statuses with a way, in rom.4's order of tests; the rest (notes, 0x41 and any other) none
constexpr Row kWays[] = {
    { 0xa0, poly }, { 0xb0, control }, { 0xe0, bend }, { 0xc0, program }, { 0xd0, pressure },
    { 0x40, parameters }, { 0x01, system }, { 0x70, reset },
};
constexpr uint8_t kRealTime = 0xf8;   // [FW] the real-time statuses from here on (the clock's): no way, as in rom.4
} // namespace

void Vc2OsMidi::controlChange(uint32_t at, uint8_t cc, uint8_t value, Vc2OsMidiParts& parts) {
    Vc2MidiPort& p = port(at);
    const auto* row = std::ranges::find(kCcWays, (int)cc, &CcRow::cc);
    (row == std::end(kCcWays) ? ccStore : row->way)(*this, p, cc, value, parts);
}

// A real-time status passes first, untouched (rom.4's table has no way for it); a channel message's (0x80…0xef) to
// its class; the system's (0xf0…0xf7) kept whole, so the table finds none of them
void Vc2OsMidi::handle(const Vc2MidiMessage& m, uint32_t word, Vc2OsMidiParts& parts, Vc2PartParams& params) {
    if (m.status >= kRealTime) return;
    const uint8_t status = m.status >= 0x80 && m.status < 0xf0 ? (uint8_t)(m.status & 0xf0) : m.status;
    const auto* row = std::ranges::find(kWays, status, &Row::status);
    if (row != std::end(kWays)) row->way(*this, m, word, parts, params);
}
