#pragma once
// The period-jump player's constants from rom.4 (v335): its routines' own pool words, as they read them; a member of
// Vc2OscTables, read once by loadVc2 at the start.  HARDWARE_FIRMWARE.md, "The VC-2's period-jump player".

// The render's (0x893caba0…0x893cabb4, 0x893ca8b0's): the crossfade's top and bottom and its scale, a sixteenth, the
// read rate's scale (2^25), a sample's scale; the set-up's (0x893cd190, 0x893ccff0's): the gain's rate
struct Vc2OscJumpPool {
    float fadeTop = 0, fadeBottom = 0, fadeScale = 0, sixteenth = 0, rateScale = 0, sampleScale = 0;
    float setUpGain = 0;
};
