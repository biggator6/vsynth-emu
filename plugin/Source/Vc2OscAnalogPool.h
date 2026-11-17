#pragma once
// The analog OSC's note-on's constants from rom.4 (v242): the pool words of 0x893c22e0 and 0x893c4a20 and the Tone
// Keyfollow table, a member of Vc2OscTables, read once by loadVc2 at the start.  Each is its routine's own pool word,
// read where it reads it; the laws are rom.2's (XtAnalog.h) but for the Pulse Width's, which the VC-2 narrows by
// waveform.  HARDWARE_FIRMWARE.md, "The VC-2's analog note-on".
#include <array>

// A waveform's Pulse Width target as rom.4 makes it from the sum x: lo + x × span
struct Vc2OscAnalogLine {
    float lo = 0, span = 0;
};

// One of SQUARE's edge steps: below `bound` (a ratio of the pitch), the step
struct Vc2OscAnalogStep {
    float bound = 0, step = 0;
};

// LA-SQUARE's and LA-SAW's own: the Hz of a ratio of 1, and the phase's step per ratio
struct Vc2OscAnalogLa {
    float hz = 0, step = 0;
};

struct Vc2OscAnalogPool {
    float impactPeriod = 0, impactBase = 0;   // 0x893c23f4 100.227, 0x893c23f8 3.05185e−5: the decay's powf
    // LA's way (0x893c2404…): Fat's half and its floor, the Pulse Width's span, the two waveforms' rows, the corner
    // from the Tone (/127, ×7 octaves, ×156) and its keyfollow's ratio 2^(9/12), the corner's most
    float laHalf = 0, laFatMin = 0, laFatFloor = 0, laPwLo = 0, laPwSpan = 0;   // 0x893c25d0…0x893c25e0
    std::array<Vc2OscAnalogLa, 2> la {};   // code 9 LA-SQUARE (0x893c25e4, 0x893c25ec), 10 LA-SAW (0x893c2604, 2608)
    float toneScale = 0, toneOctaves = 0, corner = 0, kfRatio = 0, cornerMost = 0;   // 0x893c25f0…0x893c260c
    // 2^26 and 2^31, the gains' scales; 0.002, the shapes'; the phase's start 0.25; 0.5; 4 and −2, the second phase's
    float gainScale = 0, step = 0, fullScale = 0, phaseStart = 0, half = 0, four = 0, wrap = 0;   // 0x893c2870…2890
    // the other codes' way (0x893c2780…): Fat's least, the Pulse Width's span; SAW's Slope scale, span and least
    float fatMin = 0, pwLo = 0, pwSpan = 0, sawSlope = 0, sawSpan = 0, sawLo = 0;   // 0x893c2894…0x893c2afc
    Vc2OscAnalogLine ramp, square, triangle, juno;   // 0x893c2b00, 0x893c2b08, 0x893c2b34, 0x893c2b3c
    // SQUARE's edge: no step below the least, the first below 1, then the steps' rows, else the top; its scale
    float squareLeast = 0, squareFirst = 0, squareTop = 0, edgeScale = 0;   // 0x893c2b10, 2b14, 2b2c, 2b30
    std::array<Vc2OscAnalogStep, 3> squareSteps {};   // 0x893c2b18…0x893c2b28, the last step 0x893c2af8
    float noiseSpan = 0;     // 0x893c2b44: 30, NOISE's hold
    float withinHalf = 0;    // 0x893c4a94: 0.5, 0x893c4a20's
    std::array<float, 15> toneKf {};   // 0x89546fbc: the Tone Keyfollow's factor by its value (rom.2's table)
};
