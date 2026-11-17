#pragma once
// The sub oscillator's shape's constants from rom.4 (v246, 0x893c4020): each case's own pool words, as the routine
// reads them, and its second copy of the Tone Keyfollow table; a member of Vc2OscAnalogPool, read once by loadVc2 at
// the start.  HARDWARE_FIRMWARE.md, "The VC-2's sub oscillator's shape".
#include "Vc2OscAnalogLine.h"
#include <array>

// The head's (0x893c41f0…): NOISE's ½ and 30; the gain's 2^26 and the level's 0.002
struct Vc2OscShapeHead {
    float noiseHalf = 0, noiseSpan = 0, gainScale = 0, levelStep = 0;
};

// One wave's (codes 2…7, SAW to JUNO), each word where its case reads it: the Pulse Width's line (SINE none), the Hz
// of a ratio of 1 (880 or 440), the rate (44100), the holds' scale (SAW 0.35, RAMP ⅛, else ¼) and the ½ their side
// goes by (RAMP's its own, else 0x893c4a20's), the targets' 0.002, the phases' step per ratio (220/44100), and the
// word its end takes (SQUARE's and JUNO's steepness 0.008, RAMP's segments' 1/16; the others none)
struct Vc2OscShapeWave {
    Vc2OscAnalogLine line;
    float hz = 0, fs = 0, scale = 0, half = 0, floor = 0, step = 0, end = 0;
};

// LA-SQUARE's (0x893c4a70…) and LA-SAW's (0x893c4804…): the step, 440 Hz a ratio, the Tone's /127, ×7 and ×156, the
// two corners' keyfollow ratios, the corners' most (20 kHz), ½ (LA-SAW's ratio's too), 220 Hz a ratio, 0.002
struct Vc2OscShapeLa {
    float step = 0, hz440 = 0, toneScale = 0, toneOctaves = 0, corner = 0, kf1 = 0, kf2 = 0, most = 0, half = 0;
    float hz220 = 0, floor = 0;
};

struct Vc2OscShapePool {
    Vc2OscShapeHead head;
    std::array<Vc2OscShapeWave, 6> waves {};   // codes 2…7: SAW, SQUARE, TRIANGLE, RAMP, SINE, JUNO
    std::array<Vc2OscShapeLa, 2> la {};        // codes 9 LA-SQUARE, 10 LA-SAW
    std::array<float, 15> toneKf {};           // 0x89560064: the Tone Keyfollow's factor, the shape's copy
};
