#pragma once
// vc2run sysex's probe: rom.4's SysEx receive object in the interpreter, given DT1s through its own parser, with every
// setter its parameter path ends in hooked to log what it was given, and MIDI out A's ring, where what rom.4 sends
// lands (vc2sysex.cpp; the map, vc2sysexmap.cpp; what it sends, vc2sysexsend.cpp).
#include "vc2runs.h"
#include <cstdint>
#include <map>
#include <vector>

namespace vc2::sx {

inline constexpr uint8_t kVc2Model[] = {0x00, 0x00, 0x0d};   // [FW] 0x20d in rom.4's model table (0x8950cfd0)
inline constexpr uint8_t kDeviceDefault = 0x10;             // [FW] the device byte's .data value: Device ID 17
inline constexpr uint32_t kSeven = 0x7f;

// A message to the parser: Roland's, with a four-byte address
struct Dt1 {
    std::vector<uint8_t> model{std::begin(kVc2Model), std::end(kVc2Model)};
    uint8_t device = kDeviceDefault;
    uint8_t command = 0x12;   // DT1; 0x11 is RQ1
    uint8_t address[4] = {};
    std::vector<uint8_t> data;
    bool badSum = false;      // the checksum one off
};

// What a setter was given: its group (the address tables' word +8), the zone of a zoned one (else −1), the ID it was
// handed, and from its info (0x8931c4cc, 0x8931b158) the named ID (+0x10: a value's last byte's, which names it), the
// value (+8), the data byte it came from (+4, from the message's first) and the block's number (+0: a MIC setting's
// from 1, a stored patch's number)
struct Set {
    int group;
    const char* what;
    int zone;
    uint32_t id, named, value, at, block;
    bool stored;
};

// The map as the setters gave it: by address, 7 bits a byte
struct Written {
    std::vector<Set> sets;    // as the first message to write it gave them
    uint32_t categories = 0;  // a bit for each Category that writes it
};
using Map = std::map<uint32_t, Written>;

inline uint32_t packed(uint32_t b0, uint32_t b1, uint32_t b2, uint32_t b3) {
    return b0 << 21 | b1 << 14 | b2 << 7 | b3;
}
inline uint32_t byteOf(uint32_t address, int k) { return address >> (21 - 7 * k) & kSeven; }
void printAddress(uint32_t a);

struct Probe {
    Machine& m;
    uint32_t category = 1;   // what the reads of Patch Category (0x12c) answer
    uint32_t data = 0;       // the message's first data byte, as an address
    uint32_t info = 0;       // the setter info 0x8931b110 holds in r12 at an effect object's check
    int zone = -1;           // the zone a COSM object was last made on
    int model24 = 0;         // calls of model 00 24's DT1
    bool failed = false;
    std::vector<Set> sets;
    std::vector<uint32_t> screen;   // the codes the screen's message routine was given
    std::vector<uint8_t> usb;       // what drain() found on USB out's ring
    // Builds the objects by their own routines and sets every hook; false on a fault
    bool start();
    // The message through the parser: what it set, in order (failed set on a fault)
    std::vector<Set> send(const Dt1& d);
    // The bytes put on MIDI out A's ring since the last call, a word at a time (USB out's into usb)
    std::vector<uint8_t> drain();
};

// rom.4's name for an ID
const char* idName(Machine& m, uint32_t id);

// What rom.4 sends: an RQ1's answer, and each kind's edit data for every ID against the map (vc2sysexsend.cpp)
void rq1(Probe& p);
void sends(Probe& p, const Map& map);

} // namespace vc2::sx
