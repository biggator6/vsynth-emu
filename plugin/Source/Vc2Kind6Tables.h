#pragma once
// Voice kind 6's constants from rom.4 (v225): its routines' pool words, read once by loadVc2 at the start
// (Vc2RomTables), never after.  HARDWARE_FIRMWARE.md, "Voice kind 6".
#include "Vc2Math.h"

struct Vc2Kind6Tables {
    // The setters' and the start's pool (0x893ec9d0…0x893eca04)
    float gainScale = 0;     // 2^31: P+8 into the gain's target
    float glide = 0;         // 0.1: the render's glides, a block
    float perBlock = 0;      // 1/32: the start's block of the ring
    float startDamping = 0;  // 1.2: the SVF's damping at the start
    float startWindow = 0;   // 0.5: the second head's window at the start
    // The core's (0x893ecd24…0x893ecd4c)
    float coreBlock = 0;     // 1/32: an epoch's block of the history, and the gain's glide a sample
    float ringSpan = 0;      // 4096: an epoch past the ring's end wraps
    float ringBack = 0;      // −4096
    float blockAhead = 0;    // 1.5625: the history's block is the one 50 samples past the epoch
    float liftGain = 0;      // 10: the lift's depth, (1 − r′) × this, below r′ 1
    float liftMost = 0;      // 4: at most
    float liftRate = 0;      // 0.0005: its envelope's a sample
    // T_out's and the filter's setter's (0x893ecf10…0x893ecf2c)
    float span = 0;          // 4096
    float half = 0;          // 0.5: the window's step over the shorter length; the filter's highest 1/T_out
    float damping = 0;       // 1.2: the filter's damping, less the level's share above r′ 1
    float fMost = 0;         // 1.9: f and the damping together at most
    float filterGlide = 0;   // 0.125: f's and the damping's glide, a block
    // The globals' setter's (0x893ecf40…0x893ecf4c)
    float a440Period = 0;    // 44100/440
    float unreadRate = 0;    // 880/44100
    float perA440 = 0;       // 440/44100
    Vc2MathPool math {};     // the cubic's and the window's
};

[[nodiscard]] bool vc2LoadKind6(const char* path);
bool vc2Kind6Loaded();
const Vc2Kind6Tables& vc2Kind6Tables();
