#pragma once
// What an image's effect driver is, for XtFxControl to run it: where its routines and tables are in that image, and which
// parameter IDs each of its message's blocks reads.  rom.2's (the V-Synth's) and rom.4's (the VC-2's) are one driver,
// recompiled and moved: research/VC2_MODE_PLAN.md, step 4.4.  Every address is a CPU address in its own image.
#include <array>
#include <cstdint>
#include <span>

struct XtFxImage {
    // A message's blocks.  BlockInput (v204), External Input Type and Source, has no message: its routing is a call
    enum Block : uint8_t {
        BlockSwitches = 1, BlockMfx = 2, BlockChorus = 4, BlockReverb = 8,
        BlockSystem = 16, BlockAttenuation = 32, BlockInput = 64
    };
    // The IDs first…last, which that block's reader reads: those outside the types' own tables
    struct IdRow { int first, last; Block block; };

    uint32_t self, message, clock;          // the driver's object, its message's flags, the clock's object
    uint32_t descLow, descHigh;             // parameter descriptors: 12 bytes an ID below firstHigh, then 8 bytes an ID
    int firstHigh, ids;
    uint32_t mfxIds, choIds, revIds;        // for each type, its first and last parameter's ID
    int mfxTypes, choTypes, revTypes;       // the types a patch can choose
    uint32_t staticInit, start, startTask;  // the driver's start, in the XT's order
    uint32_t takeMessage, clockPoll;        // (self): the message's readers; the poll of an outside clock
    uint32_t matrixTake, matrixApply;       // (self, values), (self): the Matrix's MFX-PRMs; 0 where nothing reaches them
    uint32_t inputRequest, inputEvent;      // the outside input's routing, () each; 0 where it isn't mapped
    uint32_t sleep, putWord, getWord;       // the bus's primitives, each hooked with what it sends, byte for byte
    uint32_t putCoefficient, putOffset;
    std::array<uint32_t, 2> reads;          // (section or 0, id) → the value
    std::array<uint32_t, 7> answered;       // the spins, and what the start touches outside the driver: they return 0
    std::span<const IdRow> idRows;
    // What the bus frame takes on its way into the chip besides the firmware's truncation (XtFxUnit, v274): rom.2's
    // tones play in XtSynth at Out Gain for TG 0, so Out Gain's default, a sixteenth, goes here (v136); rom.4's frame comes
    // as the VC-2 makes it, its tones at their own Out Gain and the mic and the vocoder added past it, so nothing
    float frameGain;
};

const XtFxImage& xtVSynthFx();   // rom.2's
const XtFxImage& vc2Fx();        // rom.4's
