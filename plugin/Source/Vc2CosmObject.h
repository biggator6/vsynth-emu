#pragma once
// rom.4's COSM object's frame (v249), bit for bit: the object's 0x164 bytes as the frame names them, one object for
// every type, two to a tone (its +0x1728 and +0x188c; the Growl's view of the same bytes is Vc2CosmGrowlWords), and the
// frame's routines: the constructor 0x893aa030 in place, the set-up 0x893aa590, the binding 0x893aa7b0 and the install
// 0x893aafe0, rom.2's chooser 0x894eef70 (the DSP routine by the code byte from the mono or the stereo table, a float
// per type and variant at +0x14c, and the flag COSM 2's install takes).  Its tables in Vc2CosmTables
// (Vc2CosmObjectPool).  The note-on, new block, update and DSP are the next versions'.  xt_vc2cosm's fifth part.
// HARDWARE_FIRMWARE.md, "The VC-2's COSM object's frame".  The tone generator's thread.
#include <array>
#include <cstdint>

// The set-up's words (+0x000…+0x00f): what 0x893aa590's struct gives, its third word the flag's 0 or 1
struct Vc2CosmSetUpWords {
    float pitch;       // +0x000: the OSC's +0x1cc, which the update writes again
    float other;       // +0x004: the OSC's +0xa80
    uint32_t flag;     // +0x008: 1 when the set-up's flag is set, else 0
    uint32_t word;     // +0x00c: the struct's fourth word
};

struct Vc2CosmObjectWords {
    Vc2CosmSetUpWords setUp;            // +0x000…+0x00f
    int32_t code;                       // +0x010: the block's code
    uint8_t variant;                    // +0x014: the block's byte the install reads: the Wave Shaper's and Amp's Type
    uint8_t unread015[3];
    uint32_t block018[11];              // +0x018…+0x043: the block's words
    int32_t filterType;                 // +0x044: the TVF's and Dynamic TVF's Filter Type, the install's for 13 and 14
    uint32_t block048[4];               // +0x048…+0x057
    uint8_t dspCode;                    // +0x058: D+0, the code's byte (the note-on's), the install's switch
    uint8_t unread059[3];
    uint32_t state[57];                 // +0x05c…+0x13f: D+4…, the type's state (the note-on's and update's)
    uint32_t pool;                      // +0x140: the buffer pool a slot was taken from (the note-on's)
    int32_t slot;                       // +0x144: its slot, −1 for none
    uint32_t routine;                   // +0x148: the DSP routine the install chose
    float factor;                       // +0x14c: the install's float for the type and variant
    std::array<uint32_t, 4> bindings;   // +0x150…+0x15f: envelopes A and B, LFOs A and B (0x893aa7b0)
    uint32_t vtable;                    // +0x160
};
static_assert(sizeof(Vc2CosmObjectWords) == 0x164, "rom.4's layout, the COSM object");

// 0x893aa030 on an object in place (its allocation, given none, is operator new's): the vtable, +0x14c 0
void vc2CosmConstruct(Vc2CosmObjectWords& o);
// 0x893aa590: the set-up's struct, its flag as 0 or 1
void vc2CosmSetUp(Vc2CosmObjectWords& o, const Vc2CosmSetUpWords& s, uint32_t flag);
// 0x893aa7b0: the envelopes' and LFOs' addresses
void vc2CosmBind(Vc2CosmObjectWords& o, const std::array<uint32_t, 4>& bindings);
// 0x893aafe0: the DSP routine and float, stereo when `flag`; 1 then, else whether it is code 10 (COSM 2's flag)
uint32_t vc2CosmInstall(Vc2CosmObjectWords& o, uint32_t flag);
