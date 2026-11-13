#pragma once
// xt_tone's cases (XT_TONE_MAIN only): XtSynth's controls drawn at random, and the patch the firmware's getter answers them
// from.  Moved from XtToneCheck.cpp as they stood (v193), for its MSM mode (XtToneMsm.cpp) to share.
#include <cstdint>
#include <unordered_map>
#include "XtSynth.h"

using OscC = XtSynth::OscControls;
using Ctl = XtSynth::Controls;

struct Rng {
    uint32_t s;
    uint32_t next() { s = s * 1103515245u + 12345u; return (s >> 8) & 0xffff; }
    int range(int lo, int hi) { return lo + (int)(next() % (uint32_t)(hi - lo + 1)); }
    bool one(int n) { return next() % (uint32_t)n == 0; }
};

// XtToneCases.cpp
int drawTime(Rng& r);
int drawRate(Rng& r);
void drawOsc(Rng& r, OscC& o);
void drawCosm(Rng& r, Ctl::Cosm& c);
void drawMatrix(Rng& r, Ctl& c);
Ctl drawControls(Rng& r, int g, bool pcm, bool pcm2 = false);
std::unordered_map<int, int> patchOf(const Ctl& c);
// and the Multi Step Modulator's IDs, 0x48d…0x4ee (v193), beside patchOf's
std::unordered_map<int, int> withMsmIds(std::unordered_map<int, int> p, const XtMsmSettings& s);
// A zone case's splits on zone 1's controls (v195): `nz` zones over the keyboard
void drawSplits(Rng& r, int nz, int (&upper)[kXtZones]);
// What a live move of the panel holds as the case started (its OSCs' kinds and waves, the Octave Shift, the zones' splits)
void holdKeys(Ctl& n, const Ctl& ctl);
// A patch's tone alone, its Multi Step Modulator off: the whole tone's cases don't run the firmware's (the MSM mode does); and
// zone 1 on every key, as a patch case gives XtSynth no zones 2…16 (v195: the upper keys are a patch's controls now)
Ctl toneAlone(Ctl c);

// xt_tone's MSM mode (XtToneMsm.cpp): `xt_tone <rom> msm [cases] [seed]`
bool xtToneMsmMode(int argc, char** argv);
int xtToneMsm(int argc, char** argv);
