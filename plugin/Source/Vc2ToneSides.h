#pragma once
// xt_vc2tone's sides (v228, v229): rom.4 and the port side by side, their compare, what each event draws on both, and
// the events.  XT_VC2TONE_MAIN only.
#include "Vc2ToneCheck.h"
#include "Vc2ToneMixRoutines.h"
#include <random>
#include <span>
#include <string>
#include <vector>

// [FW] a tone's own words the check writes on both sides, by offset: its +4 link, state, self, id, the tail's end
// routine and render flag
inline constexpr uint32_t kToneLinkAt = 0x4, kToneStateAt = 0xc, kToneSelfAt = 0x14, kToneIdAt = 0x1c;
inline constexpr uint32_t kToneEndedWordAt = 0x1b14, kToneRendersAt = 0x1b34;

struct ToneCounts {
    long events = 0, makes = 0, full = 0, inPlace = 0, unmakes = 0, back = 0, updates = 0, ends = 0, renders = 0;
    long noteOns = 0, takes = 0, follows = 0, frees = 0, fixes = 0;
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

// A word of tone i's own set on both sides: `mine`, the port's, and rom.4's at the tone's `offset`
template <class Word>
void toneBothWord(ToneSides& s, int i, uint32_t offset, Word& mine, Word value) {
    mine = value;
    s.fw.cpu.write(s.pool.addressOf(i) + offset, &value, sizeof value);
}

// The events (Vc2ToneEvents.cpp): a make, and the others drawn from the table
using ToneEvent = std::string (*)(ToneSides&, std::mt19937&);
std::string toneMakeOn(ToneSides& s, std::mt19937& rng);
std::span<const ToneEvent> toneEvents();
