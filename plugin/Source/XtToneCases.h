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
void drawOsc(Rng& r, OscC& o, bool extIn = false);
void drawCosm(Rng& r, Ctl::Cosm& c);
void drawMatrix(Rng& r, Ctl& c);
Ctl drawControls(Rng& r, int g, bool pcm, bool pcm2 = false, bool extIn = false);
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
// v361, check-pad-cc's reach, cases of their own: after the last case, a replay of the random cases with a live edit
// (g % 3 == 2) one in four, each on a generator of its own (the run's seed and the case's number) and the run's given
// back after, so no other case's draw moves. A replay has Time Trip Assign X on CC 91 and Y on the chart's 83
// (xtTonePadAssigns); where its pad then moves by those controllers, its messages go Y first and its live edit moves CC
// 91 alone, no panel moved (xtTonePadKeep: `n` kept as `ctl`), so a pad handler that didn't hear it has its finger
// elsewhere, or none, at the next message (padFields). Counted with the last section's; a replay of case g is said as
// case 100000 + g. The replays' state is the case loop's (its Report's): on once the cases are done; the run's seed,
// taken at the loop's first test; the next random case looked at; the run's generator, given back after
constexpr int kPadCc = 91, kPadAt = 4, kPadY = 83;   // kPadAt: CC 91's place in XtToneCheck's kCcNum
struct XtTonePads {
    bool seeded = false;
    bool on = false;
    uint32_t seed = 0;
    int next = 0;
    Rng saved {};
};
bool xtTonePadMore(XtTonePads& p, int& g, int end, int cases, Rng& r);   // the case loop's test: cases, then replays
int xtTonePadCaseNo(const XtTonePads& p, int g);
Ctl xtTonePadAssigns(const XtTonePads& p, Ctl c);
bool xtTonePadLive(const XtTonePads& p, const Ctl& c, bool byCc);
bool xtTonePadKeep(const XtTonePads& p, Ctl& n, const Ctl& ctl, bool byCc);

// xt_tone's MSM mode (XtToneMsm.cpp): `xt_tone <rom> msm [cases] [seed]`
bool xtToneMsmMode(int argc, char** argv);
int xtToneMsm(int argc, char** argv);
// xt_tone's modes, then `run`, the whole check (XtToneCheck.cpp): the MSM mode, or `xt_tone <rom> extin [cases] [seed]
// [inputs]`, the whole check with EXT IN drawn (v203), which the plain run doesn't draw, so its cases stay as they were
// (one generator serves the run, so a case drawn otherwise moves every one after it)
int xtToneMain(int argc, char** argv, int (*run)(int, char**, bool));
