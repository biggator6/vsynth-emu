#pragma once
// The VC-2 tone's mix of its two OSC voices (v269): rom.4's object at the tone's +0x1ab8, 0x5c bytes, whose last 0x1c are
// the words Vc2ToneTail holds from +0x40 on: the tone's index, the sync switch and mask, the render's member pointer and
// its output gain.  The constructor 0x893adbc0, the note-on 0x893adbf0, the install 0x893ade90 and the render 0x893ade50
// with its eight routines; the update 0x893ade40 does nothing.  Bit for bit: xt_vc2tone against rom.4's own routines.
// HARDWARE_FIRMWARE.md, "The VC-2's OSC mix".  Audio thread.
#include "Vc2Tone.h"
#include <array>
#include <cstdint>

struct Vc2MixWords {
    int32_t type = 0;                     // +0x00: the block's, +0x00…+0x13
    std::array<float, 4> target {};       // +0x04
    uint32_t ringA = 0, ringB = 0;        // +0x14, +0x18: the two delay lines' addresses
    std::array<float, 4> gain {};         // +0x1c
    float hold = 0.0f;                    // +0x2c
    float level = 0.0f;                   // +0x30: the follower's last
    int32_t position = 0;                 // +0x34: the delay lines' write position
    std::array<uint8_t, 4> typeByte {};   // +0x38: the note-on writes the first
    int32_t flags = 0;                    // +0x3c: which of the OSC voices' flags are set
};
static_assert(sizeof(Vc2MixWords) == 0x40, "rom.4's layout, the mix's words before the tone's tail");

// The delay lines, one pair a tone by its index: rom.4's at 0x8a0af984 and 0x8a0bb384, end to end, A's 24 then B's 24.
// A read a ring's delay puts past either end of it (OSC 2 past full scale, as an analog OSC's Impact takes it: from
// v337, its renders the voice's own) reads on into the rings beside it, as on the XT (rom.2's FM lines the same,
// XtModShared), and past them all memory the port has as 0.
class Vc2ToneMixRings {
public:
    static constexpr int kSamples = 0x1f0, kTones = 24;
    static constexpr uint32_t kAAt = 0x8a0af984, kBAt = 0x8a0bb384, kBytes = (uint32_t)kSamples * sizeof(float);
    static_assert(kBAt == kAAt + kTones * kBytes, "B's rings right after A's");
    using Ring = std::array<float, kSamples>;
    // A's ring and B's by their addresses, and the sample `i` places from `ring`'s first (v337), in it or beside it
    // (Vc2ToneMixRender.cpp)
    Ring& a(uint32_t address);
    Ring& b(uint32_t address);
    float at(const Ring& ring, int i) const;

private:
    std::array<Ring, 2 * kTones> rings_ {};
};

class Vc2ToneMix {
public:
    void construct(Vc2ToneTail& tail);
    void noteOn(const uint32_t* block, uint32_t flag1, uint32_t flag2, Vc2ToneTail& tail, Vc2ToneMixRings& rings);
    void install(bool kind3, Vc2ToneTail& tail);
    void settings(const uint32_t* block, Vc2ToneTail& tail);   // 0x893add60 (v273): a new block's targets by its type
    // The routine the install chose, on 32 stereo frames of a and b into out
    void render(const float* a, const float* b, float* out, Vc2ToneTail& tail, Vc2ToneMixRings& rings);

    Vc2MixWords w {};
};
