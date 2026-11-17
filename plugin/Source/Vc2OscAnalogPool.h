#pragma once
// The analog OSC's constants from rom.4 (v242; the update's from v248): its routines' pool words and the tables they
// read, a member of Vc2OscTables, read once by loadVc2 at the start.  Each is its routine's own pool word, read where
// it reads it: the note-on's and the update's laws are one code over each routine's own words (Vc2OscCodeWords,
// Vc2OscLaWords).  The laws are rom.2's (XtAnalog.h) but for the Pulse Width's, which the VC-2 narrows by waveform.
// HARDWARE_FIRMWARE.md, "The VC-2's analog note-on", "The VC-2's analog install, update and settings".
#include "Vc2OscAnalogLine.h"
#include "Vc2OscAnalogShapePool.h"
#include <array>

// One of SQUARE's edge steps: below `bound` (a ratio of the pitch), the step
struct Vc2OscAnalogStep {
    float bound = 0, step = 0;
};

// LA-SQUARE's and LA-SAW's own: the Hz of a ratio of 1, and the phase's step per ratio
struct Vc2OscAnalogLa {
    float hz = 0, step = 0;
};

// The plain codes' words (0…8), a routine's own: ½; Fat's 70% (the update's alone), least and floor; the Pulse Width's
// line; SAW's Slope scale, span and least; the four lines by waveform; SQUARE's edge (no step below the least, the
// first below 1, then the steps' rows, else the top; its scale); NOISE's 30
struct Vc2OscCodeWords {
    float half = 0, fatMove = 0, fatMin = 0, fatFloor = 0, pwLo = 0, pwSpan = 0, sawSlope = 0, sawSpan = 0, sawLo = 0;
    Vc2OscAnalogLine ramp, square, triangle, juno;
    float squareLeast = 0, squareFirst = 0, squareTop = 0, edgeScale = 0;
    std::array<Vc2OscAnalogStep, 3> squareSteps {};
    float noiseSpan = 0;
};

// LA's words (9, 10), a routine's own: Fat's ½, least and floor; the Pulse Width's line; the two waveforms' rows; the
// corner from the Tone (/127, ×7 octaves, ×156) and its keyfollow's ratio 2^(9/12), the corner's most; the gain's
// 2^26 and the shapes' 0.002
struct Vc2OscLaWords {
    float half = 0, fatMin = 0, fatFloor = 0, pwLo = 0, pwSpan = 0;
    std::array<Vc2OscAnalogLa, 2> la {};   // code 9 LA-SQUARE, 10 LA-SAW
    float toneScale = 0, toneOctaves = 0, corner = 0, kfRatio = 0, cornerMost = 0, gainScale = 0, step = 0;
};

// The sub oscillator's note-on's own (v244, 0x893c3b90): 2^31, the gain's scale; 0.002, the shapes'; 1/32, the
// level's; ½; −2, the phases' wrap
struct Vc2OscAnalogSubPool {
    float fullScale = 0, step = 0, levelStep = 0, half = 0, wrap = 0;   // 0x893c3c20, 3c24, 3c28, 3e7c, 3e80
};

inline constexpr size_t kVc2AnalogCodes = 11;   // the waveform codes 0…10 the installs' tables hold

struct Vc2OscAnalogPool {
    float impactPeriod = 0, impactBase = 0;   // 0x893c23f4 100.227, 0x893c23f8 3.05185e−5: the decay's powf
    Vc2OscLaWords la, updateLa;               // the note-on's (0x893c2404…) and the update's (0x893c31d0)
    Vc2OscCodeWords codes, updateCodes;       // the note-on's (0x893c2780…) and the update's (0x893c2d70…)
    // the note-on's: 2^31, the gain's; the phase's start 0.25; 4 and −2, the second phase's
    float fullScale = 0, phaseStart = 0, four = 0, wrap = 0;   // 0x893c2880…2890
    float withinHalf = 0;    // 0x893c4a94: 0.5, 0x893c4a20's
    std::array<float, 15> toneKf {};   // 0x89546fbc: the Tone Keyfollow's factor by its value (rom.2's table)
    // the installs' (v248): each code's float for +0xa88, the sub oscillator's off and on
    std::array<float, kVc2AnalogCodes> installed {}, subInstalled {};   // 0x895422f8, 0x89542324
    Vc2OscAnalogSubPool sub;           // the sub oscillator's note-on's (v244)
    Vc2OscShapePool shape;             // the sub oscillator's shape's (v246, Vc2OscAnalogShapePool.h)
};
