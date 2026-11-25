#pragma once
// HQ-SAW's and HQ-SQUARE's tables (v337): the band-limited impulse A (±4608 floats by its centre) and its window B
// (±8192), end to end in both images, which the renders reach through two pointers set at startup (rom.2's
// 0x8987c898 and 0x8987c89c, rom.4's 0x8993fbf8 and 0x8993fbfc).  Null: rom.2's, as xtAnalogLoadTables reads them;
// else another image's copy, the VC-2's rom.4's (Vc2OscAnalogPool), which its renders on XtAnalogOsc are given.

struct XtAnalogHq {
    static constexpr int kAHalf = 4608, kBHalf = 8192, kFloats = 2 * kAHalf + 1 + 2 * kBHalf + 1;
    const float* a = nullptr;   // A's centre
    const float* b = nullptr;   // B's centre
};

// The impulse's scale for a block (HQ's k2: A's step per unit of x) with the tables the tap reads
struct XtAnalogHqScale {
    float k = 0;
    XtAnalogHq t;
};
