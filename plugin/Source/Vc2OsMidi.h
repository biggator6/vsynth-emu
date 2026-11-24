#pragma once
// rom.4's OS MIDI task's handler and the MIDI channel states (v313, step 4.3's forty-first version, item 10's
// eighteenth part): what the OS library does with each message before the part's dispatcher (v308's 0x8938fed0) sees
// it, and on the XT before, as the hub 0x89314d30 waits for the OS's queue to empty.  Its handler 0x89314e90 by the
// status's class: poly pressure, control change (0x893172e0: Receive Bank Select's gate on CC 0 and 32, Data Entry's
// two halves, the NRPN's and RPN's numbers, CC 121's reset, a value below 99 stored, the listeners told), program
// change (0x893172a0: the listeners told while Receive Program Change is on), channel pressure, the bend, 0x70's reset
// of a source's 16 channels (0x89317780) and the 0x40 messages by their word's kind (kind 0: the full reset of all 16
// channels on byte 1's bit 7, 0x893178a0 with 0x893176d0; kind 4: the two Receive flags read, 0x893178d0); and a
// channel state's routines (0x89317a10…0x89317cf0) on any state, the static M's own (v295) too.  The 32 states (rows 0
// and 1 of v276's table 0x8950d6e0) and the 48 port objects (0x8950d530's) word for word, with their static
// initializers 0x89317d00 and 0x89317910.  The OS's part objects (the listeners), its other routines the 0x40 and 0x01
// messages run, and SysEx's flag through Vc2OsMidiParts; the flags' read through v286's Vc2PartParams.  Not in the
// sound yet.  Bit for bit: xt_vc2midi.  HARDWARE_FIRMWARE.md, "rom.4's MIDI channel states".
#include "Vc2MidiDispatch.h"
#include "Vc2PartSettings.h"
#include <array>
#include <cstdint>

// A MIDI channel's state, 0xec bytes as rom.4 lays them: the part reads it at its +2 (C), its controllers at C+0x84
struct Vc2ChannelState {
    uint8_t channel = 0;                        // +0x00: 0…15 (the static M's 0)
    uint8_t unread01 = 0;
    uint16_t bend = 0;                          // +0x02 (C+0x00): 14 bits, centre 0x2000
    uint8_t pressure = 0;                       // +0x04 (C+0x02): the channel pressure
    uint8_t polyMost = 0;                       // +0x05 (C+0x03): the most of the poly pressures
    std::array<uint8_t, 128> poly {};           // +0x06 (C+0x04): poly pressure by key
    // +0x86 (C+0x84): CC 0…98 by number, 96…98 also the bend up, the bend down and the last pressure (kBendUp…), then
    // the template's last byte, which no routine writes
    std::array<uint8_t, 100> controllers {};
    std::array<uint8_t, 2> pad {};
    static constexpr int kBendUp = 96, kBendDown = 97, kLastPressure = 98, kStored = 99;
};
static_assert(sizeof(Vc2ChannelState) == 0xec, "rom.4's layout, a MIDI channel state");

// A port object: a source's and channel's view of a channel state, with its RPN's and NRPN's numbers
struct Vc2MidiPort {
    uint32_t state = 0;       // +0x0: its channel state, by address
    uint16_t rpn = 0;         // +0x4: MSB << 8 | LSB, bit 15 not selected, 0x7f7f none
    uint16_t nrpn = 0;        // +0x6: the same
    uint16_t data = 0;        // +0x8: Data Entry, MSB << 7 | LSB
    uint16_t unread0a = 0;
    uint32_t listeners = 0;   // +0xc: the first listener's node, 0 none
};
static_assert(sizeof(Vc2MidiPort) == 16, "rom.4's layout, a port object");

// What the handler calls out: the OS's part objects that listen on a port (a node's object and the node after it,
// and the object's virtual at a slot, given two words), its other routines a 0x40 or 0x01 message runs (given the
// message's word), and SysEx's flag (0x8992e3d1)
class Vc2OsMidiParts {
public:
    static constexpr uint32_t kProgramSlot = 0x0c, kControllerSlot = 0x18, kRpnSlot = 0x24, kNrpnSlot = 0x30;
    // [FW] osRoutine's: the OS's routines a 0x40 or 0x01 message runs, each given the word's address (SysEx's, the
    // last, its object 0x8995dd0c and the word); the checks hook them (v346: xt_vc2synth's too)
    static constexpr uint32_t kOsRoutines[] = { 0x89317fe0, 0x89318420, 0x89318210, 0x89318040, 0x89318450,
                                                0x893154a0, 0x8931a420, 0x8931a460, 0x89310640 };
    virtual ~Vc2OsMidiParts() = default;
    virtual uint32_t listenerOf(uint32_t node) = 0;    // the node's +0
    virtual uint32_t nextListener(uint32_t node) = 0;  // its +0xc
    virtual void heard(uint32_t object, uint32_t slot, uint32_t a, uint32_t b) = 0;
    virtual void osRoutine(uint32_t routine, uint32_t word) = 0;
    virtual uint8_t sysexOn() = 0;
};

// A channel state's routines, on any state
void vc2ChannelSetUp(Vc2ChannelState& s, uint8_t channel);       // 0x89317a10
void vc2ChannelFullReset(Vc2ChannelState& s);                     // 0x89317a50: the template and defaults again
void vc2ChannelResetControllers(Vc2ChannelState& s);              // 0x89317a90
void vc2ChannelDefault(Vc2ChannelState& s, int cc);               // 0x89317b30
void vc2ChannelStore(Vc2ChannelState& s, int cc, uint8_t value);  // 0x89317c20: below 99
void vc2ChannelBend(Vc2ChannelState& s, uint8_t lsb, uint8_t msb);    // 0x89317c60
void vc2ChannelPoly(Vc2ChannelState& s, uint8_t key, uint8_t value);  // 0x89317cc0: keys below 128
void vc2ChannelPressure(Vc2ChannelState& s, uint8_t value);       // 0x89317cf0

class Vc2OsMidi {
public:
    static constexpr uint32_t kPortsAt = 0x8995eaf4, kStatesAt = 0x8995edf8, kFlagsAt = 0x8992e3e8;   // [FW]
    static constexpr int kPorts = 48, kStates = 32;
    static constexpr uint16_t kNone = 0x7f7f;   // [FW] no RPN or NRPN

    void staticInitStates();    // 0x89317d00 (on 0x8995edf4): rows 0 and 1, each channel set up
    void staticInitPorts();     // 0x89317910 (on 0x8995eaf0): rows 0…2 on the states of sources 1…3
    // 0x89314e90: a message (its source the row of port objects) and the word beside it
    void handle(const Vc2MidiMessage& m, uint32_t word, Vc2OsMidiParts& parts, Vc2PartParams& params);
    void controlChange(uint32_t port, uint8_t cc, uint8_t value, Vc2OsMidiParts& parts);   // 0x893172e0
    void programChange(uint32_t port, uint8_t program, Vc2OsMidiParts& parts);             // 0x893172a0
    void resetSource(uint8_t source);       // 0x89317780: Reset All Controllers on its 16 channels
    void resetChannel(int channel);         // 0x893176d0: the channel's state set up again, by sources 1 and 3
    void resetAll(uint32_t word);           // 0x893178a0: every channel's, on the word's byte 1 bit 7
    void readFlags(Vc2PartParams& params);  // 0x893178d0: Receive Program Change and Bank Select

    uint32_t portOf(uint8_t source, int channel) const;   // a source's and channel's port object, by address
    Vc2MidiPort& port(uint32_t at) { return ports[(at - kPortsAt) / sizeof(Vc2MidiPort)]; }
    Vc2ChannelState& state(uint32_t at) { return states[(at - kStatesAt) / sizeof(Vc2ChannelState)]; }

    // Each listener on the port given the slot's virtual; Data Entry's value with its RPN's, then its NRPN's; a
    // controller stored and told (the control change's ways, Vc2OsMidiHandler.cpp)
    void tellAll(const Vc2MidiPort& p, Vc2OsMidiParts& parts, uint32_t slot, uint32_t a, uint32_t b);
    void tellData(const Vc2MidiPort& p, Vc2OsMidiParts& parts);
    void storeAndTell(Vc2MidiPort& p, uint8_t cc, uint8_t value, Vc2OsMidiParts& parts);
    void nulled(Vc2MidiPort& p) { p.rpn = p.nrpn = kNone; }

    std::array<Vc2MidiPort, kPorts> ports {};
    std::array<Vc2ChannelState, kStates> states {};
    uint8_t receiveProgram = 0, receiveBank = 0;   // 0x8992e3e8, 0x8992e3e9
};
