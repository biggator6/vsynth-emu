#pragma once
// xt_vc2midi's shared parts (v313): the two sides (rom.4's interpreter, with the OS's listening part objects' virtuals,
// its other routines the 0x40 and 0x01 messages run and the parameters' getter hooked to log; the port's Vc2OsMidi and
// its fake parts reading the same listeners from rom.4's memory), the calls logged, the counts and the events.
// XT_VC2MIDI_MAIN only.
#include "Vc2CheckMain.h"
#include "Vc2OsMidi.h"
#include <functional>
#include <span>
#include <vector>

inline constexpr uint32_t kMidiMessageAt = 0x8C030000, kMidiWordAt = 0x8C030004;   // a message and its word
inline constexpr uint32_t kMidiNodesAt = 0x8C031000, kMidiObjectsAt = 0x8C032000;   // the listeners' nodes, objects
inline constexpr uint32_t kMidiVtableAt = 0x8C033000;   // the objects' one vtable: each slot's adjustment and routine
inline constexpr uint32_t kMidiSlotsAt = 0x8C034000;    // the slots' routines, one at this + the slot, hooked
inline constexpr uint32_t kMidiSysexAt = 0x8992e3d1;    // [FW] SysEx's flag byte
inline constexpr uint32_t kMidiStatesThis = 0x8995edf4, kMidiPortsThis = 0x8995eaf0;   // [FW] the initializers' this
inline constexpr int kMidiNodes = 6, kMidiObjects = 3, kMidiEvents = 18;
inline constexpr uint32_t kMidiSlots[] = { Vc2OsMidiParts::kProgramSlot, Vc2OsMidiParts::kControllerSlot,
                                           Vc2OsMidiParts::kRpnSlot, Vc2OsMidiParts::kNrpnSlot };

// What a call was: a listener's virtual (its routine, the object it was given, its two words; a program change's
// second word is the routine's own leftover, logged as 0), one of the OS's routines (the message's word, and for
// SysEx's the object it is given), a parameter read (its ID)
enum MidiKind : int32_t { kMidiHeard, kMidiOs, kMidiRead };

struct MidiCall {
    int32_t kind;
    uint32_t routine, a, b, c;
    bool operator==(const MidiCall&) const = default;
};

struct MidiCounts {
    long events = 0;
    std::array<long, kMidiEvents> byEvent {};
    long heard = 0, os = 0, reads = 0, byStatus = 0;
};

// The two sides: rom.4's interpreter with its hooks, the port's OS MIDI, both logs, the reads' answers drawn (Receive
// Program Change's, Receive Bank Select's) and the counts
struct MidiSides {
    explicit MidiSides(Vc2Fw& rig);
    Vc2Fw& fw;
    Vc2OsMidi port {};
    std::vector<MidiCall> rom, mine;
    std::array<int32_t, 2> answers {};
    std::array<uint32_t, std::size(kMidiSlots)> deltas {};   // each slot's adjustment in the listeners' vtable
    MidiCounts n {};
};

// rom.4's routine on `args` after the port's ports, states and flags are written into its memory, then the port's
// `mine(parts, params)`; what differs after: the ports, the states, the flags, the calls
std::string midiOnBoth(MidiSides& s, const Vc2CheckRoutine& r, std::initializer_list<uint32_t> args,
                       const std::function<void(Vc2OsMidiParts&, Vc2PartParams&)>& mine);

using MidiEvent = std::string (*)(MidiSides&, std::mt19937&);
std::span<const MidiEvent> midiEvents();

// A state and a port object of the 32 and 48, by address; a source of the five
uint32_t midiAnyState(std::mt19937& rng);
uint32_t midiAnyPort(std::mt19937& rng);
uint8_t midiAnySource(std::mt19937& rng);
// The draws before an event: the ports' states and numbers, their listeners, the flags, SysEx's flag, the answers
void midiDraw(MidiSides& s, std::mt19937& rng);
// Half the time a port's state (now and then), numbers, Data Entry and listeners drawn: an event's own port first
void midiSteerPort(MidiSides& s, uint32_t at, std::mt19937& rng);
// Half the time a state's bend bytes and last pressure drawn (0 or any 7 bits): an event's own state first
void midiSteerState(MidiSides& s, uint32_t at, std::mt19937& rng);
