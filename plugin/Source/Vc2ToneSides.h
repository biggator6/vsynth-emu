#pragma once
// xt_vc2tone's sides (v228, v229): rom.4 and the port side by side, their compare, what each event draws on both, and
// the events.  XT_VC2TONE_MAIN only.
#include "Vc2ToneCheck.h"
#include "Vc2ToneMixRoutines.h"
#include <random>
#include <span>
#include <string>
#include <vector>

// [FW] a tone's own words the check writes on both sides, by offset: its state, self, id, the tail's end routine,
// truncation's routine (v273) and render flag
inline constexpr uint32_t kToneNextAt = 0x0, kTonePrevAt = 0x4;   // its links in the part's lists (v273)
inline constexpr uint32_t kToneStateAt = 0xc, kToneSelfAt = 0x14, kToneIdAt = 0x1c;
inline constexpr uint32_t kToneEndedWordAt = 0x1b14, kToneTruncatedWordAt = 0x1b18, kToneRendersAt = 0x1b34;

struct ToneCounts {
    long events = 0, makes = 0, full = 0, inPlace = 0, unmakes = 0, back = 0, updates = 0, ends = 0, renders = 0;
    long noteOns = 0, takes = 0, fixes = 0;
    long freeTicks = 0, freeClocks = 0;   // v271: the free LFOs' two passes
    long setters = 0, keyOffs = 0, truncations = 0, waits = 0, costs = 0, getters = 0;   // v273: the remaining methods
    // v315: the envelopes' settings in sustain, the LFOs' fades a release starts, the released envelopes an update ticks
    long sustains = 0, releaseFades = 0, releaseTicks = 0;
    long starts = 0, started = 0, listMoves = 0, leaves = 0;   // the part's lists: starts, tones started, moves, leaves
    long bounds = 0;   // v268: the words the OSC voice and the TVA read
    long mixPlays = 0;                                     // v269: a mix's note-on, install and blocks run alone
    std::array<long, kVc2MixRoutines.size()> mixBlocks {};   // and the blocks each of its eight renders ran
};

// Both sides: rom.4 with its calls' log, the port's pool, its parts (one a tone) and their owner, the tones made
struct ToneSides {
    Vc2Fw& fw;
    Vc2TonePool& pool;
    ToneShared& shared;
    std::vector<TonePortParts>& parts;
    TonePortOwner& owner;
    std::vector<ToneCall>& rom;
    std::vector<int> live {};
    ToneCounts n {};
};

// The pool's words, the tones' own and part words (every tone's, or tone `only`'s), the note-on's input and the free
// objects, then the calls; empty when nothing differs
std::string toneDifference(ToneSides& s, int only);
// Before an event on tone i: the hooked routines' answers and the tone's part words drawn and written on both sides,
// the free objects cleared, the logs fresh
void tonePrepare(ToneSides& s, int i, std::mt19937& rng);
// Before a note-on on tone i besides: the input drawn (the legato's tone and matches, the fix-up's case) and the two
// lists built from the tones made, on both sides
void tonePrepareNoteOn(ToneSides& s, int i, std::mt19937& rng);

// The real envelopes and LFOs (v268, Vc2ToneModCheck.cpp): tone i's objects against rom.4's, empty when they agree; the
// blocks of a note-on's input as the converters write them; and the words the tone reads of them, drawn on both sides
std::string toneModulatorsDifference(ToneSides& s, int i);
void toneDrawModulatorBlocks(Vc2ToneInput& in, std::mt19937& rng);
void toneDrawModulatorWords(ToneSides& s, int i, std::mt19937& rng);
// v315: what an event about to run on tone i reaches of the envelopes' and LFOs' settings and release, counted: an
// envelope's settings in sustain (half the time a started one put there on both sides, as its ticks would leave it, so
// few of the check's tones tick that far), the fades a key-off's releases start, the released envelopes an update ticks
void toneSteerSustain(ToneSides& s, int i, uint32_t envelope, std::mt19937& rng);
void toneCountReleases(ToneSides& s, int i);
void toneCountReleaseTicks(ToneSides& s, int i);

// The real OSC mix (v269, Vc2ToneMixCheck.cpp): tone i's mix's words against rom.4's; the delay lines of tone i, or of
// every tone; the mix's block of a note-on's input; a render's buffers drawn on both sides, with a routine installed
// first if the tone has none (rom.4 would call address 0); what a render leaves; and the mix played alone
std::string toneMixDifference(ToneSides& s, int i);
std::string toneMixRingsOf(ToneSides& s, int i);
std::string toneMixRingsDifference(ToneSides& s);
void toneDrawMixBlock(Vc2ToneInput& in, std::mt19937& rng);
std::string toneMixPrepareRender(ToneSides& s, int i, std::mt19937& rng);
std::string toneMixRendered(ToneSides& s, int i);
std::string toneMixPlayOn(ToneSides& s, std::mt19937& rng);

// The real free LFOs (v271, Vc2FreeLfoCheck.cpp): the manager against rom.4's words; built afresh on both sides; whether a
// note-on's LFOs can all take an object (rom.4 reads from address 0 past the last); an LFO's +0x7c as the tone may find
// it; and the manager's two passes
std::string toneFreeLfosDifference(ToneSides& s);
std::string toneFreeLfosFresh(ToneSides& s);
bool toneFreeLfosHaveRoom(ToneSides& s);
uint32_t toneDrawFollow(std::mt19937& rng);
std::string toneFreeTickOn(ToneSides& s, std::mt19937& rng);
std::string toneFreeClockOn(ToneSides& s, std::mt19937& rng);

// The tone's remaining methods and the part's lists (v273, Vc2ToneMethodCheck.cpp): the lists' words against rom.4's;
// both sides' lists emptied; tone i taken off whichever list holds it by rom.4's own routine, before a use that builds
// it again (a note-on, a constructor) and after its destructor, as the part keeps a tone on one list at most; and the
// events: a block setter, key-off, a truncation, the cost, a getter, the start and the lists' routines alone
std::string toneListsDifference(ToneSides& s);
void toneEmptyLists(ToneSides& s);
std::string toneLeaveLists(ToneSides& s, int i);
void toneDrawLinks(ToneSides& s, int i, std::mt19937& rng);   // a tone on no list: links its constructor must clear
std::string toneSetterOn(ToneSides& s, std::mt19937& rng);
std::string toneKeyOffOn(ToneSides& s, std::mt19937& rng);
std::string toneTruncateOn(ToneSides& s, std::mt19937& rng);
std::string toneCostOn(ToneSides& s, std::mt19937& rng);
std::string toneGetterOn(ToneSides& s, std::mt19937& rng);
std::string toneStartOn(ToneSides& s, std::mt19937& rng);
std::string toneListOn(ToneSides& s, std::mt19937& rng);

// A live tone drawn and prepared, its number
inline int toneDrawLive(ToneSides& s, std::mt19937& rng) {
    const int i = s.live[rng() % s.live.size()];
    tonePrepare(s, i, rng);
    return i;
}

// An event on tone i: rom.4's routine on `args` and the port's `mine()`, then what differs on it
template <class Mine>
std::string toneOn(ToneSides& s, int i, uint32_t at, std::initializer_list<uint32_t> args, Mine mine) {
    return vc2BothOn(s.fw, at, args, "a tone's routine", mine, [&] { return toneDifference(s, i); });
}

// A word of tone i's own set on both sides: `mine`, the port's, and rom.4's at the tone's `offset`
template <class Word>
void toneBothWord(ToneSides& s, int i, uint32_t offset, Word& mine, Word value) {
    mine = value;
    s.fw.cpu.write(s.pool.addressOf(i) + offset, &value, sizeof value);
}

// A live tone drawn and prepared, sounding but one time in `odds`, its number
inline int toneDrawSounding(ToneSides& s, std::mt19937& rng, uint32_t odds) {
    const int i = toneDrawLive(s, rng);
    if (rng() % odds != 0) toneBothWord(s, i, kToneStateAt, s.pool.tones[(size_t)i].head.state, Vc2Tone::kSounding);
    return i;
}

// The events (Vc2ToneEvents.cpp): a make, and the others drawn from the table
using ToneEvent = std::string (*)(ToneSides&, std::mt19937&);
std::string toneMakeOn(ToneSides& s, std::mt19937& rng);
std::span<const ToneEvent> toneEvents();
