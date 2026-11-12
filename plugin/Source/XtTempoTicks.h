#pragma once
// The tempo clock's ticks, 480 a quarter (v189): the clock object at 0x89979bfc (built by 0x89471040) counts them each ms
// (0x894710e0), and its task gives the OS's tempo timer (0x8930eb00), which the arpeggio steps on, those since the last
// time (0x894710b0).  INTERNAL: 24 units a ms, as ticks by 20 / the quarter's ms, with the remainder kept.  EXTERNAL: each F8
// takes the ticks on to the next 20 (0x894712f0); between F8s they ramp at 120 over the last six clocks' ms, never to the
// next 20.  The 24 clocks are every 20th tick; XtSynth counts those its own way (clocksDue), this is the arpeggio's.
// HARDWARE_FIRMWARE.md, "The arpeggiator in XtSynth".  Checked against the routines by xt_arp (XtTempoTicksCheck.cpp).
#include <cstdint>

struct XtTempoTicks {
    static constexpr int kPerClock = 20;     // +0x10: ticks a clock, 24 clocks a quarter
    uint16_t speed = 0x4000;                 // +0x38: the units' rate, 16384ths of 24 a ms
    int32_t units = 0;                       // +0x1c: 24 a ms
    uint16_t unitFrac = 0;                   // +0x20: its 16384ths
    int32_t unitsTaken = 0;                  // +0x24: units turned into ticks
    uint16_t remainder = 0;                  // +0x2c: what the last division left, in units × 20
    uint32_t ticks = 0;                      // +0x28
    uint32_t fed = 0;                        // +0x34: ticks given to the tempo timer
    uint16_t ramp = 0;                       // +0x4a: EXTERNAL, ticks past the last F8 in 256ths
    void msInternal(int quarterMs);          // 0x89471160; quarterMs: +0x14 (XtSynth::clockPeriod)
    void msExternal(int six, int since);     // 0x894710fc; six: +0x48, the ms over the last six clocks; since: +0x46
    void f8();                               // 0x8947130c
    int32_t take();                          // 0x894710b0: the ticks since the last take
};
