// XtSynth's part's note-on: a new voice, a legato, or the voices truncated first.  Moved from XtSynth.cpp as it stood (v193).
#include "XtSynth.h"
#include <algorithm>
#include <cmath>

// The part's note-on (0x894dbcb0): the key state moves on (0x894d6330),
// the key takes the Octave Shift (and the system's Transpose, 0 here).
// POLY: a new voice.  MONO with a key already held and Legato on: the
// sounding voice takes the note (0x894d4e00).  MONO otherwise: the part's
// voices are truncated (0x894d4a80) and a new one starts, on the key's zone.  A legato note keeps the
// sounding voices' zones (0x894d4e00 takes no zone).  The allocator's head (0x894d4af0) starts the Multi Step Modulator
// again by its Key Sync, after MONO's truncation and before the voice is taken; a legato doesn't reach it.
void XtSynth::tgNoteOn(int note, float velocity) {
    pickUpSample();
    const int key = shiftKey(note), zone = xtZoneOf(zoneUpper, key);
    if (!canPlay(zone)) return;
    keys.press(note);
    keys.noteOn(note, (int32_t)std::floor((double)hostFrames * 1000.0 / hostRate));
    const int vel = (int)std::lround(std::clamp(velocity, 0.0f, 1.0f) * 127.0f);
    if (ctl.mono) {
        if (keys.prevMonoHeld && ctl.legato) { legatoNote(key, vel); return; }
        for (Voice& v : voice) if (v.active) truncate(v);
    }
    msmNoteOn();
    startVoice(note, key, vel, zone);
    for (Voice& v : voice) xtCosmWordsRelease(v);   // the COSMs' input words its pass held (matrixNoteOn)
}
