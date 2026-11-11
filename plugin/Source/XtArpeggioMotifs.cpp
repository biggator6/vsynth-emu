// Which key a step plays (XtArpeggio.h): the eight motifs (0x89534a9c's table), the walks through the keys held that they
// choose by, and the octave each starts its note in.  The keys are an array by key (+0x108) for UP, DOWN and the walk by
// count, and a list in the order played (+0x104) for NOTE-ORDER, PHRASE and RHYTHM.  A walk skips a key whose note is
// sounding, and gives none while as many notes sound as keys are held.  HARDWARE_FIRMWARE.md, "The arpeggiator".
#include "XtArpeggio.h"

namespace {
constexpr int kLast = XtArpeggio::kKeys - 1;
constexpr int kNoteBits = 0x7f;
constexpr uint16_t kSeedMul = 0x497d;   // both seeds' step: seed · 0x497d + 1 [FW 0x893021e8, 0x893035a0]

int keysListed(const XtArpeggio& a) {
    int n = 0;
    if (a.keyHead < 0) return 0;
    int16_t k = a.keyHead;
    do {
        k = a.keys[k].next;
        ++n;
    } while (k != a.keyHead);
    return n;
}

bool tooFew(const XtArpeggio& a) {
    return keysListed(a) <= a.soundingCount;
}

int take(XtArpeggio& a, int k) {
    a.curKey = a.keys[k].key;
    a.curNode = (int16_t)k;
    return k;
}

// 0x89302030 (up, +1) and 0x893020f0 (down, −1): from the key last played round the array, the next held and not sounding
int walk(XtArpeggio& a, int by) {
    if (tooFew(a)) return -1;
    int k = a.curKey;
    for (int n = 0; n < XtArpeggio::kKeys; ++n) {
        k += by;
        if (k > kLast) k = 0;
        if (k < 0) k = kLast;
        if (a.keys[k].vel == 0) continue;
        if (!xtArpBit(a.sounding, take(a, k))) return k;
    }
    return k;
}

// The n-th key held (from 0), counting from key 1 round to key 0 (0x8930222a, 0x893023c8)
int nthHeld(XtArpeggio& a, int n) {
    int k = 0;
    do {
        k = k == kLast ? 0 : k + 1;
        if (a.keys[k].vel != 0) --n;
    } while (n >= 0);
    return take(a, k);
}

uint16_t nextSeed(uint16_t& seed) {
    seed = (uint16_t)(seed * kSeedMul + 1);
    return seed;
}

// 0x893021b0
int randomKey(XtArpeggio& a) {
    if (tooFew(a)) return -1;
    const int draw = (nextSeed(a.keySeed) >> 8) & 0xff;
    return nthHeld(a, draw % keysListed(a));
}

// 0x89302260: the key played after the last, skipping those sounding, 129 at most
int orderKey(XtArpeggio& a) {
    if (tooFew(a)) return -1;
    int k = take(a, a.curNode >= 0 ? a.keys[a.curNode].next : a.keyHead);
    for (int n = 129; n > 0 && xtArpBit(a.sounding, a.keys[k].key); --n) k = a.curNode = a.keys[a.curNode].next;
    return take(a, k);
}

// 0x89302340: line i plays the (i mod count)-th key held; with too few keys, only lines below the count
int autoKey(XtArpeggio& a, int line) {
    const int n = keysListed(a);
    if (n <= a.soundingCount && line >= n) return -1;
    return nthHeld(a, line % n);
}

// The octave a note starts in, within 0…Octave Range: UP's starts again at the bottom (0x893034e0), DOWN's at the top
// (0x89303530), RANDOM's anywhere (0x89303580)
int octaveFrom(XtArpeggio& a, bool fromTop) {
    const int lo = a.octaveRange < 0 ? a.octaveRange : 0, hi = a.octaveRange < 0 ? 0 : a.octaveRange;
    if (a.lastNote >= 0 && a.octave >= lo && a.octave <= hi) return a.octave;
    a.octave = (int8_t)(fromTop ? hi : lo);
    return a.octave;
}

int octaveRandom(XtArpeggio& a) {
    const int lo = a.octaveRange < 0 ? a.octaveRange : 0, hi = a.octaveRange < 0 ? 0 : a.octaveRange;
    const int draw = (nextSeed(a.octaveSeed) >> 8) & 0xff;
    a.octave = (int8_t)(lo + draw % (hi - lo + 1));
    return a.octave;
}

void play(XtArpeggio& a, int octave, int k) {
    a.playNote(octave * 12 + a.keys[k].key, a.keys[k].vel);
}

// Whether a key above (+1) or below (−1) k is held
bool heldBeyond(const XtArpeggio& a, int k, int by) {
    for (k += by; k >= 0 && k <= kLast; k += by)
        if (a.keys[k].vel != 0) return true;
    return false;
}

// 0x89302dd0 UP and 0x89302ea0 DOWN: past the last key the octave moves on
void upOrDown(XtArpeggio& a, int by) {
    const int k = walk(a, by);
    if (k < 0) return;
    play(a, octaveFrom(a, by < 0), k);
    a.octavePending = 0;
    if (!heldBeyond(a, k, by)) a.octave = (int8_t)(a.octave + by);
    a.direction = (int8_t)by;
}

void up(XtArpeggio& a) { upOrDown(a, 1); }
void down(XtArpeggio& a) { upOrDown(a, -1); }

// 0x89302f60 UP&DOWN: past the last key the octave moves on, or at the range's end the walk turns
void upAndDown(XtArpeggio& a) {
    const int by = a.direction < 0 ? -1 : 1;
    const int k = walk(a, by);
    if (k < 0) return;
    play(a, octaveFrom(a, true), k);
    a.octavePending = 0;
    if (heldBeyond(a, k, by)) return;
    const int end = by > 0 ? (a.octaveRange < 0 ? 0 : a.octaveRange) : (a.octaveRange < 0 ? a.octaveRange : 0);
    if (by > 0 ? a.octave < end : a.octave > end)
        a.octave = (int8_t)(a.octave + by);
    else
        a.direction = (int8_t)-by;
}

// 0x893030e0
void randomOrder(XtArpeggio& a) {
    const int k = randomKey(a);
    if (k < 0) return;
    play(a, octaveRandom(a), k);
    a.octavePending = 0;
}

// 0x893031a0: after the last key played the octave moves on
void noteOrder(XtArpeggio& a) {
    const int k = orderKey(a);
    if (k < 0) return;
    play(a, octaveFrom(a, false), k);
    a.octavePending = 0;
    if (a.keyHead >= 0 && k == a.keys[a.keyHead].prev) a.octave = (int8_t)(a.octave + 1);
}

// PHRASE (0x89303210) and RHYTHM (0x893032a0): the last key played, at the last velocity; the pattern's octave moves on
// at its end.  PHRASE plays the line's note moved to the key as line 1's is; RHYTHM the line's note as it is.
void lastKeyLine(XtArpeggio& a, bool phrase) {
    if (a.keyHead < 0) return;
    const int k = take(a, a.keys[a.keyHead].prev);
    const int octave = octaveFrom(a, false);
    const int lineNote = a.pattern.line[a.line] & kNoteBits;
    const int note = phrase ? a.keys[k].key + lineNote - (a.pattern.line[0] & kNoteBits) : lineNote;
    a.playNote(octave * 12 + note, a.lastVelocity);
    a.octavePending = 1;
}

void phrase(XtArpeggio& a) { lastKeyLine(a, true); }
void rhythm(XtArpeggio& a) { lastKeyLine(a, false); }

// 0x89303320 AUTO: each line its own key, from the lowest; the pattern's octave moves on at its end
void autoLines(XtArpeggio& a) {
    const int k = autoKey(a, a.line);
    if (k < 0) return;
    play(a, octaveFrom(a, false), k);
    a.octavePending = 1;
}

// 0x89534a9c, by Motif
using Motif = void (*)(XtArpeggio&);
constexpr Motif kMotifs[8] = { up, down, upAndDown, randomOrder, noteOrder, rhythm, phrase, autoLines };
} // namespace

// 0x893041e0: a Motif past AUTO is AUTO
void XtArpeggio::playLine() {
    kMotifs[motif > 7 ? 7 : motif](*this);
}
