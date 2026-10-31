#pragma once
// The V-Synth XT's key state per part and MIDI channel (0x24 bytes each, from
// 0x89cf52d8; reset by 0x894d6420): the keys portamento glides from and the
// key a MONO part falls back on.  Keys are as played, before the Octave
// Shift.
//   +0x0c  the sounding key
//   +0x10  the time of the last note-on (the system clock, ms), +0x14 the
//          chord window, +0x1e the time since the note-on before
//   +0x1a  POLY: the previous key, +0x1b whether a key was held then;
//   +0x1c  the last key, +0x1d whether a key is held
//   +0x20  MONO: the previous key, +0x21 whether a key was held then;
//   +0x22  the last key, +0x23 whether a key is held
// The held keys are a bitmap in the channel's state (bit k & 15 of word
// k >> 4), set and cleared before these run.
#include <cstdint>

struct XtKeyState {
    int cur = 0;
    int32_t lastMs = 0;
    int window = 0x20;
    int prevPoly = 60, lastPoly = 60, prevMono = 60, lastMono = 60;
    bool prevPolyHeld = false, lastPolyHeld = false, prevMonoHeld = false, lastMonoHeld = false;
    uint16_t held[8] = {};

    void press(int key) { held[(key & 127) >> 4] |= (uint16_t)(1u << (key & 15)); }
    void release(int key) { held[(key & 127) >> 4] &= (uint16_t)~(1u << (key & 15)); }

    // 0x894d66b0: the highest held key in lo…hi, or −1.
    int highestHeld(int lo, int hi) const {
        for (int k = hi; k >= lo; --k)
            if (held[k >> 4] & (1u << (k & 15))) return k;
        return -1;
    }

    // Note-on (0x894d6330), at the system clock's ms.  A note-on less than
    // the window after the one before (a chord) leaves the POLY previous key
    // where it was and narrows the window by 1 ms; a later one moves it on
    // and resets the window to 32 ms.  The MONO previous key always moves on.
    void noteOn(int key, int32_t ms) {
        uint32_t dt = (uint32_t)(ms - lastMs);
        if (dt > 0xffff) dt = 0xffff;
        lastMs = ms;
        if (dt < (uint32_t)window) --window;
        else {
            window = 0x20;
            prevPoly = lastPoly;
            prevPolyHeld = lastPolyHeld;
        }
        prevMono = lastMono;
        prevMonoHeld = lastMonoHeld;
        lastMono = lastPoly = cur = (int8_t)key;
        lastMonoHeld = lastPolyHeld = true;
    }

    // Note-off (0x894d63c0), after the key left the bitmap: with no key held,
    // the held flags clear and the result is −1.  When the sounding key is
    // released, the highest key still held takes its place and is returned
    // (a MONO part plays it, 0x894dbea0); otherwise −1.
    int noteOff(int key) {
        const int r = highestHeld(0, 127);
        if (r < 0) {
            lastMonoHeld = prevMonoHeld = lastPolyHeld = prevPolyHeld = false;
            return -1;
        }
        if ((int8_t)cur != (int8_t)key) return -1;
        cur = r;
        prevMono = lastMono;
        lastMono = r;
        prevPolyHeld = lastPolyHeld;
        return r;
    }
};
