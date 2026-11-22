// rom.4's OS MIDI task's handler and the MIDI channel states: see Vc2OsMidi.h (v313).  Each routine's reads, calls and
// writes in rom.4's order.
#include "Vc2OsMidi.h"
#include "Vc2KeysTables.h"
#include "Vc2OsMidiTables.h"
#include <algorithm>
#include <cstring>
#include <iterator>

namespace {
using S = Vc2ChannelState;
constexpr uint16_t kCentre = 0x2000;         // [FW] the bend's centre
constexpr int kSetUpOrder[] = { 7, 10, 93, 91 };   // [FW] the controllers a set-up gives their defaults, in order
// [FW] Reset All Controllers' controllers, in order (0x89317a90)
constexpr int kResetOrder[] = { 11, 1, 2, 34, 16, 17, 18, 19, 48, 49, 50, 51, 80, 81, 82, 83, 64, 66, 67, 69 };

// [FW] a controller's default (0x89317b30): Volume, Expression and the two sends full, Pan centred, the rest 0
struct Default {
    int cc;
    uint8_t value;
};
constexpr Default kDefaults[] = { { 7, 127 }, { 10, 64 }, { 93, 127 }, { 91, 127 }, { 11, 127 } };
} // namespace

void vc2ChannelDefault(S& s, int cc) {
    const auto* d = std::ranges::find(kDefaults, cc, &Default::cc);
    const uint8_t value = d == std::end(kDefaults) ? 0 : d->value;
    if (cc < S::kStored) s.controllers[(size_t)cc] = value;
}

void vc2ChannelStore(S& s, int cc, uint8_t value) {
    if (cc < S::kStored) s.controllers[(size_t)cc] = value;
}

void vc2ChannelResetControllers(S& s) {
    for (const int cc : kResetOrder) vc2ChannelDefault(s, cc);
    s.bend = kCentre;   // 0x89317bd0
    s.controllers[S::kBendUp] = 0;
    s.controllers[S::kBendDown] = 0;
    s.poly.fill(0);     // 0x89317bf0
    s.polyMost = 0;
    s.controllers[S::kLastPressure] = 0;
    s.pressure = 0;
    s.controllers[S::kLastPressure] = 0;
}

void vc2ChannelFullReset(S& s) {
    const auto& t = vc2OsMidiTables().stateTemplate;
    std::memcpy(&s.bend, t.data(), t.size());   // C…C+0xe7
    for (const int cc : kSetUpOrder) vc2ChannelDefault(s, cc);
    vc2ChannelResetControllers(s);
}

void vc2ChannelSetUp(S& s, uint8_t channel) {
    s.channel = channel;
    vc2ChannelFullReset(s);
}

void vc2ChannelBend(S& s, uint8_t lsb, uint8_t msb) {
    const auto bend = (uint16_t)((uint32_t)msb << 7 | lsb);
    s.bend = bend;
    if (bend == kCentre) {
        s.controllers[S::kBendUp] = 0;
        s.controllers[S::kBendDown] = 0;
    } else if (bend > kCentre) {
        s.controllers[S::kBendUp] = (uint8_t)((bend - kCentre) >> 6);
    } else {
        s.controllers[S::kBendDown] = (uint8_t)((kCentre - 1 - bend) >> 6);
    }
}

void vc2ChannelPoly(S& s, uint8_t key, uint8_t value) {
    s.poly[key] = value;
    uint8_t most = value;
    for (const uint8_t p : s.poly) most = std::max(most, p);
    s.polyMost = most;
    s.controllers[S::kLastPressure] = most;
}

void vc2ChannelPressure(S& s, uint8_t value) {
    s.pressure = value;
    s.controllers[S::kLastPressure] = value;
}

uint32_t Vc2OsMidi::portOf(uint8_t source, int channel) const {
    const Vc2OsMidiTables& t = vc2OsMidiTables();
    return t.ports[(size_t)(t.portRows[source] * 16 + channel)];
}

void Vc2OsMidi::staticInitStates() {
    const auto& rows = vc2KeysTables().states;
    for (int c = 0; c < 16; ++c)
        for (const int row : { 0, 1 })
            if (const uint32_t at = rows[(size_t)(row * 16 + c)]; at != 0) vc2ChannelSetUp(state(at), (uint8_t)c);
}

void Vc2OsMidi::staticInitPorts() {
    const Vc2OsMidiTables& t = vc2OsMidiTables();
    const Vc2KeysTables& k = vc2KeysTables();
    for (int c = 0; c < 16; ++c)
        for (int row = 0; row < 3; ++row) {
            const uint32_t at = t.ports[(size_t)(row * 16 + c)];
            if (at == 0) continue;
            Vc2MidiPort& p = port(at);
            p.state = k.states[(size_t)(k.sources[(size_t)row] * 16 + c)];
            nulled(p);
            p.listeners = 0;
        }
}

void Vc2OsMidi::tellAll(const Vc2MidiPort& p, Vc2OsMidiParts& parts, uint32_t slot, uint32_t a, uint32_t b) {
    for (uint32_t node = p.listeners; node != 0; node = parts.nextListener(node))
        parts.heard(parts.listenerOf(node), slot, a, b);
}

// Data Entry's value to each listener: its RPN's, then its NRPN's
void Vc2OsMidi::tellData(const Vc2MidiPort& p, Vc2OsMidiParts& parts) {
    for (uint32_t node = p.listeners; node != 0; node = parts.nextListener(node)) {
        parts.heard(parts.listenerOf(node), Vc2OsMidiParts::kRpnSlot, p.rpn, p.data);
        parts.heard(parts.listenerOf(node), Vc2OsMidiParts::kNrpnSlot, p.nrpn, p.data);
    }
}

void Vc2OsMidi::storeAndTell(Vc2MidiPort& p, uint8_t cc, uint8_t value, Vc2OsMidiParts& parts) {
    vc2ChannelStore(state(p.state), cc, value);
    tellAll(p, parts, Vc2OsMidiParts::kControllerSlot, cc, value);
}

void Vc2OsMidi::programChange(uint32_t at, uint8_t program, Vc2OsMidiParts& parts) {
    if (receiveProgram != 0) tellAll(port(at), parts, Vc2OsMidiParts::kProgramSlot, program, 0);
}

void Vc2OsMidi::resetSource(uint8_t source) {
    for (int c = 0; c < 16; ++c) {
        Vc2MidiPort& p = port(portOf(source, c));
        vc2ChannelResetControllers(state(p.state));
        nulled(p);
    }
}

void Vc2OsMidi::resetChannel(int channel) {
    for (const uint8_t source : { 1, 3 }) {
        Vc2MidiPort& p = port(portOf(source, channel));
        vc2ChannelFullReset(state(p.state));
        nulled(p);
    }
}

void Vc2OsMidi::resetAll(uint32_t word) {
    if ((word >> 8 & 0x80) == 0) return;
    for (int c = 0; c < 16; ++c) resetChannel(c);
}

void Vc2OsMidi::readFlags(Vc2PartParams& params) {
    constexpr int16_t kProgramId = 0x45, kBankId = 0x46;   // [FW] Receive Program Change, Receive Bank Select
    receiveProgram = (uint8_t)params.value(Vc2ParamBlock::kSystemB, 0, 0, kProgramId);
    receiveBank = (uint8_t)params.value(Vc2ParamBlock::kSystemB, 0, 0, kBankId);
}
