#pragma once
// SOLO's constants from rom.4 (v334): each routine's own pool words, as it reads them; a member of Vc2OscTables, read
// once by loadVc2 at the start.  HARDWARE_FIRMWARE.md, "The VC-2's SOLO".

// The block's rates' (0x893c9b30…0x893c9b50, 0x893c94a0's): with the Energy 0 the cutoff's scale and the Q; with it on,
// the rates the lift bends at; the coefficient's cubic and its scale, the glide to the new coefficient and Q, the most
// they sum to.  The frame's and the grains' (0x893c9b20…0x893c9b2c, 0x893c9b54): a window's half and its scale, an
// age's unit (1/4096), a sixteenth, a sample's scale.  The grain parameters' (0x893c9da0…0x893c9dc8, 0x893c9c60's):
// a period's floor; Robot Voice's glide, its swing's most and its scale; the rate's floor, the trigger's most and its
// scale, the anchor step's scale and floor, the window's steps, the cutoff's scale and most.  The repeat's cost
// (0x893c9f40…0x893c9f58, 0x893c9dd0's): the anchor step's and the walk's units, the two lines and the scale.  The
// set-ups' (0x893cc610…0x893cc624): the gain's rate, an age's window unit, the second window's start, the Q
struct Vc2OscSoloPool {
    float offScale = 0, offQ = 0, liftLow = 0, liftTop = 0, cubic = 0, cubicBase = 0, coefScale = 0, glide = 0;
    float sumMost = 0;
    float windowHalf = 0, windowScale = 0, ageUnit = 0, sixteenth = 0, sampleScale = 0;
    float periodFloor = 0, robotGlide = 0, robotSwing = 0, robotScale = 0, rateFloor = 0, triggerMost = 0;
    float triggerScale = 0, stepScale = 0, windowSteps = 0, cutoffScale = 0, cutoffMost = 0;
    float costAnchor = 0, costWalk = 0, costLowSlope = 0, costLowBase = 0, costHighSlope = 0, costHighBase = 0;
    float costScale = 0;
    float setUpGain = 0, setUpWindow = 0, setUpHalf = 0, setUpQ = 0;
};
