#pragma once
// rom.4's Matrix sources (v245): the sixteen routines the refresh calls for a source's value (0x893a1de0…0x893a21f0),
// by their rom.4 addresses, which M's routine words hold, as Vc2MatrixSourceRoutines over what they read: M's head
// bytes, the channel's state its +0x168 points at, the controller assignments (0x89997398 + 0x848…0x84e) and its part's
// byte; their values through the curve (Vc2MatrixTables, 0x895401f6); and the choice 0x893a1c70, a source parameter's
// routine.  Bit for bit: xt_vc2cosm's third part against rom.4's own.  HARDWARE_FIRMWARE.md, "The VC-2's Matrix
// sources".  The tone generator's thread.
#include "Vc2Matrix.h"
#include <array>
#include <cstdint>

// A channel's state as the sources read it, at M's +0x168, word for word to the farthest read (a controller's value by
// a number of 255: +0x183)
struct Vc2MatrixChannel {
    uint16_t bend;                  // +0x000: 0…16383, its value less 8192
    uint8_t aftertouch;             // +0x002: the channel's
    uint8_t unread3;
    uint8_t polyAftertouch[128];    // +0x004…+0x083: by key (a key byte past 127 reads on into the controllers)
    uint8_t controllers[256];       // +0x084…+0x183: by number
};
static_assert(sizeof(Vc2MatrixChannel) == 0x184, "rom.4's layout, a channel's state as the Matrix's sources read it");

// What the sources read besides M's words: the channel, the seven assignments and the part's byte (the part table
// 0x8993d040 by M's +0x16c, its +0x24)
struct Vc2MatrixSourceState {
    const Vc2MatrixWords* m = nullptr;          // its head: +0 the velocity, +1 the key, +2 the aftertouch's key
    const Vc2MatrixChannel* channel = nullptr;
    std::array<uint8_t, 7> assigns {};          // 0x89997398 + 0x848…0x84e: a controller's number, 0 or 32 none
    uint8_t partByte = 0;
};

// The sources by rom.4's routine addresses; 0 for an address that is none of them
class Vc2MatrixSourceRoutines final : public Vc2MatrixSources {
public:
    explicit Vc2MatrixSourceRoutines(const Vc2MatrixSourceState& state) : state_(state) {}
    int32_t value(uint32_t routine, uint8_t byte) override;

private:
    const Vc2MatrixSourceState& state_;
};

// 0x893a1c70: a source parameter's routine (0 none, 1…95 a controller by number, 96 bend, 97 aftertouch, 98…109 the
// others, past them none)
uint32_t vc2MatrixSourceRoutine(int32_t source);
