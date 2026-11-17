#pragma once
// A waveform's Pulse Width target as rom.4 makes it from the sum x: lo + x × span (v242; its own header from v246,
// the analog note-on's pool and the sub oscillator's shape's both holding lines)

struct Vc2OscAnalogLine {
    float lo = 0, span = 0;
};
