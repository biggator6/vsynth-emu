// xt_vc2midi <rom.4> [cases] [seed] (v313): rom.4's OS MIDI task's handler and its channel states, Vc2OsMidi, against
// rom.4's own routines on its own 32 states, 48 port objects and two Receive flags in .bss.  The OS's listening part
// objects are objects in scratch memory with one vtable (each slot's adjustment drawn, its routine at a hooked address
// logging the object it is given and its words); the OS's nine other routines a 0x40 or 0x01 message runs and the
// parameters' getter 0x89324350 are hooked to log (the getter answering Receive Program Change and Receive Bank Select
// as drawn, both sides alike), the read objects' set-ups 0x89320760 and 0x89320790 to do nothing.  At the start both
// static initializers on both sides, compared; then each case 400 events (Vc2OsMidiEvents.cpp): the handler on a drawn
// message of every class, and each routine alone.  Before each the draws: now and then a state's bytes, a port's state
// (one of the 32), numbers, Data Entry and listeners (a chain of up to three nodes), the flags, SysEx's flag and the
// answers.  After each the ports, the states, the flags and the calls.  Not drawn: data bytes past 127 (a key there
// writes past the pressures), a source past 4 (rom.4 reads past its row table).  XT_VC2MIDI_FIRST=1 says where the
// first case to differ does, XT_VC2MIDI_STOP=1 stops there.  XT_VC2MIDI_MAIN only.
#ifdef XT_VC2MIDI_MAIN
#include "Vc2CheckCalls.h"
#include "Vc2KeysTables.h"
#include "Vc2OsMidiCheck.h"
#include "Vc2OsMidiTables.h"
#include <cstring>

namespace {
using M = Vc2OsMidi;
constexpr int kSteps = 400;
constexpr uint32_t kSysexRoutine = 0x89310640, kSysexObject = 0x8995dd0c;   // [FW] SysEx's routine, its object
constexpr uint32_t kGetterAt = 0x89324350;                                  // [FW] the parameters' getter
constexpr uint32_t kSetUpsAt[] = { 0x89320760, 0x89320790 };                // [FW] the read objects' set-ups
// [FW] the OS's routines a 0x40 or 0x01 message runs, each given the word's address (SysEx's the object and the word)
constexpr uint32_t kOsRoutines[] = { 0x89317fe0, 0x89318420, 0x89318210, 0x89318040, 0x89318450,
                                     0x893154a0, 0x8931a420, 0x8931a460, kSysexRoutine };
constexpr int16_t kReadIds[] = { 0x45, 0x46 };   // [FW] the two reads' IDs

int32_t answerFor(const MidiSides& s, uint32_t id) {
    const auto* at = std::ranges::find(kReadIds, (int16_t)id);
    return at == std::end(kReadIds) ? 0 : s.answers[(size_t)(at - std::begin(kReadIds))];
}

// The port's parts: the listeners read from rom.4's memory as its routines read them, the calls logged as its hooks log
class MidiPortParts final : public Vc2OsMidiParts, public Vc2PartParams {
public:
    explicit MidiPortParts(MidiSides& s) : s_(s) {}
    uint32_t listenerOf(uint32_t node) override { return s_.fw.cpu.rd32(node); }
    uint32_t nextListener(uint32_t node) override { return s_.fw.cpu.rd32(node + 0xc); }
    void heard(uint32_t object, uint32_t slot, uint32_t a, uint32_t b) override {
        const uint32_t vtable = s_.fw.cpu.rd32(object + 4);
        const uint32_t self = object + s_.fw.cpu.rd32(vtable + slot), routine = s_.fw.cpu.rd32(vtable + slot + 8);
        s_.mine.push_back({ kMidiHeard, routine, self, a, slot == kProgramSlot ? 0 : b });
    }
    void osRoutine(uint32_t routine, uint32_t word) override {
        s_.mine.push_back({ kMidiOs, routine, word, routine == kSysexRoutine ? kSysexObject : 0, 0 });
    }
    uint8_t sysexOn() override { return s_.fw.cpu.rd8(kMidiSysexAt); }
    int32_t value(Vc2ParamBlock, int, int, int16_t id) override {
        s_.mine.push_back({ kMidiRead, kGetterAt, (uint32_t)id, 0, 0 });
        return answerFor(s_, (uint32_t)id);
    }
    int32_t cosmValue(int, int, int, int16_t) override { return 0; }
    bool held(int) override { return false; }

private:
    MidiSides& s_;
};

std::string said(const MidiCall& c) {
    char t[96];
    std::snprintf(t, sizeof t, "%d %08x (%08x %08x %08x)", c.kind, c.routine, c.a, c.b, c.c);
    return t;
}

// What differs: the ports, the states, the two flags, the calls
std::string midiDifference(MidiSides& s) {
    std::string d = vc2RegionsDifference(s.fw, {
        { M::kPortsAt, s.port.ports.data(), sizeof s.port.ports, "the port objects" },
        { M::kStatesAt, s.port.states.data(), sizeof s.port.states, "the channel states" },
    });
    const uint8_t flags[2] = { s.fw.cpu.rd8(M::kFlagsAt), s.fw.cpu.rd8(M::kFlagsAt + 1) };
    if (d.empty() && (flags[0] != s.port.receiveProgram || flags[1] != s.port.receiveBank))
        d = "the Receive flags: " + std::to_string(s.port.receiveProgram) + " " + std::to_string(s.port.receiveBank) +
            ", against " + std::to_string(flags[0]) + " " + std::to_string(flags[1]);
    return d.empty() ? vc2CallsDifference(s.rom, s.mine, said) : d;
}

void writePort(MidiSides& s) {
    s.fw.cpu.write(M::kPortsAt, s.port.ports.data(), sizeof s.port.ports);
    s.fw.cpu.write(M::kStatesAt, s.port.states.data(), sizeof s.port.states);
    s.fw.cpu.wr8(M::kFlagsAt, s.port.receiveProgram);
    s.fw.cpu.wr8(M::kFlagsAt + 1, s.port.receiveBank);
}

// The start: both static initializers on both sides, from the port's noise
std::string midiStart(MidiSides& s, std::mt19937& rng) {
    std::vector<uint8_t> noise(sizeof s.port.states);
    for (uint8_t& b : noise) b = (uint8_t)rng();
    std::memcpy(s.port.states.data(), noise.data(), noise.size());
    std::string what = midiOnBoth(s, { 0x89317d00, "the states' initializer" }, { kMidiStatesThis },
                                  [&](Vc2OsMidiParts&, Vc2PartParams&) { s.port.staticInitStates(); });
    if (what.empty())
        what = midiOnBoth(s, { 0x89317910, "the port objects' initializer" }, { kMidiPortsThis },
                          [&](Vc2OsMidiParts&, Vc2PartParams&) { s.port.staticInitPorts(); });
    return what;
}

std::string midiCase(MidiSides& s, std::mt19937& rng) {
    std::string what = midiStart(s, rng);
    const auto events = midiEvents();
    for (int k = 0; k < kSteps && what.empty(); ++k, ++s.n.events) {
        midiDraw(s, rng);
        what = events[rng() % events.size()](s, rng);
        s.n.heard += vc2CountCalls(s.mine, kMidiHeard);
        s.n.os += vc2CountCalls(s.mine, kMidiOs);
        s.n.reads += vc2CountCalls(s.mine, kMidiRead);
    }
    return what;
}

void sayMidiCounts(const Vc2Run& run, const MidiCounts& n, long different) {
    const auto& e = n.byEvent;
    std::printf("midi: %d cases, %ld events: %ld handled (%ld by status's way), %ld control changes, %ld program "
                "changes, %ld set-ups, %ld full resets, %ld controller resets, %ld defaults, %ld stores, %ld bends, "
                "%ld poly pressures, %ld pressures, %ld source resets, %ld channel resets, %ld resets of all, %ld "
                "flag reads, %ld and %ld initializers; %ld listeners told, %ld OS routines, %ld reads\n",
                run.cases, n.events, e[0], n.byStatus, e[1], e[2], e[3], e[4], e[5], e[6], e[7], e[8], e[9], e[10],
                e[11], e[12], e[13], e[14], e[15], e[16], n.heard, n.os, n.reads);
    std::printf("xt_vc2midi: %ld different\n", different);
}
} // namespace

MidiSides::MidiSides(Vc2Fw& rig) : fw(rig) {
    for (const uint32_t slot : kMidiSlots)
        fw.cpu.hook(kMidiSlotsAt + slot, [this, slot](Sh4& c) {
            rom.push_back({ kMidiHeard, kMidiSlotsAt + slot, c.r[4], c.r[5],
                            slot == Vc2OsMidiParts::kProgramSlot ? 0 : c.r[6] });
            c.ret(0);
        });
    for (const uint32_t routine : kOsRoutines)
        fw.cpu.hook(routine, [this, routine](Sh4& c) {
            const bool sysex = routine == kSysexRoutine;
            rom.push_back({ kMidiOs, routine, sysex ? c.r[5] : c.rd32(c.r[4]), sysex ? c.r[4] : 0, 0 });
            c.ret(0);
        });
    for (const uint32_t at : kSetUpsAt) fw.cpu.hook(at, [](Sh4& c) { c.ret(0); });
    fw.cpu.hook(kGetterAt, [this](Sh4& c) {
        rom.push_back({ kMidiRead, kGetterAt, c.r[5], 0, 0 });
        c.ret((uint32_t)answerFor(*this, c.r[5]));
    });
}

std::string midiOnBoth(MidiSides& s, const Vc2CheckRoutine& r, std::initializer_list<uint32_t> args,
                       const std::function<void(Vc2OsMidiParts&, Vc2PartParams&)>& mine) {
    writePort(s);
    s.rom.clear();
    s.mine.clear();
    MidiPortParts parts { s };
    const std::string what = vc2BothAnswer(s.fw, r, args, [&] {
        mine(parts, parts);
        return 0u;
    }, [&] { return midiDifference(s); });
    return what.empty() ? what : std::string(r.name) + ": " + what;
}

int main(int argc, char** argv) {
    const Vc2CheckStart start { "xt_vc2midi", vc2LoadAll<vc2LoadKeys, vc2LoadOsMidi>,
                                { "XT_VC2MIDI_FIRST", "XT_VC2MIDI_STOP" } };
    return vc2CheckMain<MidiSides>(argc, argv, start, midiCase, sayMidiCounts);
}
#endif
