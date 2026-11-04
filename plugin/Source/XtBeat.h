#pragma once
// What a VariPhrase player does first in each block (rom.2 0x8950c280): the walk's
// beat.  It keeps the wave's position apart from the read position, and steers the
// walk by a factor at +0x8c:
//   no Tempo Sync, no Time Trip: the factor is 1, the walk as the knobs have it;
//   Tempo Sync (byte 0 bit 1): each clock moves the target by one clock of the wave's
//     Original Tempo, and the factor follows the error;
//   Time Trip (bit 2): each step of the pad does the same to its own target, and the
//     factor falls to 0 when the walk has got there, so the wave stands still.
// The beat position (+0x64) runs on regardless, at the walk's speed and through the
// loop: it is where a released Time Trip goes back to with Beat Keep on.
// Positions are in 1/512 sample (0x4000 in a block of 32).  Single precision as the
// SH-4 computes it (XtFloat.h).  Defined in XtMultiBandPlayer.cpp, which every user links.
#include <cstdint>

struct XtBeat {
    int8_t clk = 0, trip = 0;                       // +0x40, +0x41: clocks and Time Trip steps not yet taken
    uint32_t orig = 0, tempo = 0;                   // +0x48 the wave's Original Tempo, +0x4c the note's, ×10000
    int32_t step = 0, budget = 0;                   // +0x50 one clock at the Original Tempo, +0x54
    int32_t syncPos = 0, tripPos = 0, walkPos = 0, beatPos = 0;   // +0x58, +0x5c, +0x60, +0x64
    float factor = 1.0f, integ = 0.0f;              // +0x8c, +0x90

    // 0x8950f860, a note's start; `pos16` the player's position (+0x38, 1/16 sample)
    void start(uint32_t tempo_, uint32_t orig_, int32_t pos16);
    // 0x8950f900, after a seek: the factor 0 under Time Trip, else the tempo's
    void reset(bool tripOn);
    // The block's walk before the player's own flags: flags0 is the player's byte 0, lp the
    // cached loop events' positions (forward start, forward end, backward end, backward
    // start).  Returns the walk step as a float, walk·factor·2^25; ftrc it for +0x34.
    float block(uint8_t flags0, float walk, const int32_t* lp);

private:
    void advance(int32_t by, float walk, const int32_t* lp);
    float steer(int32_t err, int sign);
};
