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
    long told = 0;   // v317: the renders whose told fade ended, the TVA setting the OS's event
    long cosmSlots = 0, cosmFreed = 0, cosmRenders = 0;   // v318: slots taken and freed, whole renders through a COSM
    long cosmStale = 0;   // the stale slots a destructor would have freed, named none first
    long cosmModesKept = 0;   // the install passes alone that would have switched a sounding COSM's mode
    long cosmFollowers = 0;   // whole renders through a stereo Side Band Filter, whose follower reads past buffer c
    long starts = 0, started = 0, listMoves = 0, leaves = 0;   // the part's lists: starts, tones started, moves, leaves
    long bounds = 0;   // v268: the words the OSC voice and the TVA read
    long mixPlays = 0;                                     // v269: a mix's note-on, install and blocks run alone
    std::array<long, kVc2MixRoutines.size()> mixBlocks {};   // and the blocks each of its eight renders ran
    // v319: the OSC voices' note-ons by kind, the settings that reached a voice, the mic's blocks before kind 6's renders,
    // and the calls they made beyond the tone, by xt_vc2osc's kinds of call
    std::array<long, 8> oscKinds {};
    long oscSettings = 0, micBlocks = 0, oscSteadied = 0;   // and the OSC 1s steadied for a COSM that takes its pitch
    long extInBlocks = 0, variOffBlocks = 0;   // v322, v323: the tones' renders with an OSC through each own render
    long headsBlocks = 0;                      // v326: and through LITE's heads'
    long liteSettings = 0, bothVari = 0;   // v324: LITE's settings on a type with a way, tones drawn two VariPhrases
    long modeOneSetUps = 0, modeOneTaken = 0;   // v328: mode 1's set-ups at an install, those through a take's head
    long settingsTaken = 0;                     // v328: and the LITE settings through a take's head
    long modeOneSeeks = 0, bandReseeks = 0;     // v329: the OSCs steered for mode 1's seek, VariPhrase's re-seeks
    long variDrawn = 0;                         // v330: VariPhrase's update's words drawn finite
    long variByEvents = 0;                      // v331: its note-ons by events, their table steered
    std::array<long, kOscKinds> oscCalls {};
};

// v318: each tone's COSMs' slots (−1 none) its last note-on took and its destructor hasn't freed, and whether it has had
// a note-on since it was built, which its COSMs' sounds need before a whole render
struct ToneCosmTrack {
    std::array<std::array<int32_t, 2>, Vc2TonePool::kTones> slots {};
    std::array<bool, Vc2TonePool::kTones> noted {};
    void forget() {
        for (auto& s : slots) s = { -1, -1 };
        noted = {};
    }
};

// Both sides: rom.4 with its calls' logs (v319: the OSC voices' calls beyond the tone, its own), the port's pool, its
// parts (one a tone) and their owner, the tones made
struct ToneSides {
    Vc2Fw& fw;
    Vc2TonePool& pool;
    ToneShared& shared;
    std::deque<TonePortParts>& parts;
    TonePortOwner& owner;
    std::vector<ToneCall>& rom;
    std::vector<OscCall>& oscRom;
    std::vector<int> live {};
    ToneCounts n {};
    ToneCosmTrack cosm {};   // v318
};

// The pool's words, the tones' own and parts' words (every tone's, or tone `only`'s), the note-on's input and the free
// objects, then the calls; empty when nothing differs
std::string toneDifference(ToneSides& s, int only);
// Before an event on tone i: the hooks' answers and the parts' words drawn and written on both sides, the logs fresh
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

// The real TVA (v317, Vc2ToneTvaCheck.cpp): tone i's TVA against rom.4's words; its words drawn and written on both
// sides as xt_vc2tva draws them (Vc2TvaDraws), bound to the tone's own envelope and LFOs, now and then stopped, and
// the fade's step; a note-on's TVA block, its envelope's and LFOs' numbers kept; and a render's output, drawn on both
// sides before and compared after
std::string toneTvaDifference(ToneSides& s, int i);
void toneDrawTva(ToneSides& s, int i, std::mt19937& rng);
void toneDrawTvaBlock(Vc2ToneInput& in, std::mt19937& rng);
void toneDrawOut(ToneSides& s, std::mt19937& rng);
std::string toneOutDifference(ToneSides& s);

// The real COSMs (v318, Vc2ToneCosmCheck.cpp): tone i's two objects against rom.4's, but the words their sounds keep (the
// port keeps those in XtCosm's objects, v263) and the stack bytes the set-up's struct leaves; the pools' words; each
// event the objects bound to the tone's own envelopes and LFOs and the three words a COSM's render reads past buffer c;
// a note-on's COSM blocks as their converters write them, and the setter's new block, its code the object's three
// times in four; before a note-on, the slots the tone's last one took freed on both sides, as the part's destructor
// would have; after it, its slots and that it has a note; a tone built or unmade, none
std::string toneCosmDifference(ToneSides& s, int i);
std::string tonePoolsDifference(ToneSides& s);
void toneDrawCosms(ToneSides& s, int i, std::mt19937& rng);
void toneDrawCosmBlocks(Vc2ToneInput& in, std::mt19937& rng);
void toneDrawCosmNewBlock(ToneSides& s, int i, uint32_t cosm, std::mt19937& rng);
void toneCosmsBeforeNoteOn(ToneSides& s, int i, std::mt19937& rng);
void toneNoted(ToneSides& s, int i);
void toneUnnoted(ToneSides& s, int i, bool freed);
// Before a destructor: a COSM holding no slot by the check's count names none on both sides, so the destructor frees no
// slot another tone holds (the part destroys only tones it gave a note)
void toneHoldNoStaleSlots(ToneSides& s, int i);
// Before a note-on that starts no COSMs on a tone with a note: COSM 1's mode kept as its note chose it, OSC 1's block an
// EXT IN's whose flag gives it (rom.4 installs at the note-on alone, which starts the COSMs; v319)
void toneKeepCosmMode(ToneSides& s, int i, std::mt19937& rng);
// Whether COSM 1's sound is a stereo Side Band Filter, whose follower reads the three words past buffer c
bool toneCosmFollows(ToneSides& s, int i);
// A case's start: both sides' pools set up again by rom.4's static initializer and the port's, no tone noted or holding a
// slot; what differs after the set-up
std::string toneFreshCosms(ToneSides& s);

// The real OSC voices (v319, Vc2ToneOscCheck.cpp): tone i's two voices against rom.4's memory, whole; the calls they
// make beyond the tone, xt_vc2osc's hooks' against its fake parts'.  Before every event, the hooks' answers and the OS's
// clock drawn, the calls' logs fresh; before a note-on, its OSC blocks, records and ramp words as xt_vc2osc draws them
// (Vc2OscDraws), each block's bound parts' numbers the tone's own; a block made a kind's (that kind's words with it)
// where the legato, the fix-up or a COSM's mode wants it; an OSC voice made analog on both sides; the analog's object
// words on a voice about to take an analog note; the vibratos' fades held for an update's count; a block of the mic
// before a render with a voice of kind 6; the setter's settings block, an analog's words in it; the ramp words alone;
// the mic ring set up once and each case
std::string toneOscDifference(ToneSides& s, int i);
std::string toneOscCallsDifference(ToneSides& s);
void toneDrawOscs(ToneSides& s, std::mt19937& rng);
void toneDrawOscBlocks(ToneSides& s, std::mt19937& rng);
void toneOscBlockOfKind(ToneSides& s, int osc, uint32_t kind, uint32_t wave, std::mt19937& rng);
// An OSC block made an EXT IN's (kind 4) of a neutral pitch, its ratio 1 at the root's 440 Hz, its flag 1 for the
// wave's type 1 alone, and the input written again: what a COSM 1 that takes OSC 1's pitch needs (the Side Band
// Filters, the Growl), and what keeps COSM 1's mode at a note-on that starts no COSMs (v319)
void toneOscSteady(ToneSides& s, int osc, uint32_t wave, std::mt19937& rng);
void toneMakeOscAnalog(ToneSides& s, int i, int osc, uint32_t code, std::mt19937& rng);
// v328 (Vc2ToneSides.cpp): whether tone i's OSC runs mode 1's set-up at an install (LITE's kind, type 1); and its
// records, head and tempo words steered on both sides as its note-on leaves them (oscSteerModeOneSetUp)
bool toneOscModeOne(ToneSides& s, int i, int osc);
void toneSteerModeOne(ToneSides& s, int i, int osc);
// v329 (Vc2ToneSides.cpp): tone i's OSC voices before an event that can reach mode 1's seek (0x893cc830) on their own
// object, their mode 1 words drawn on both sides (oscSteerModeOneSeek) and counted: before a note-on those whose block
// is LITE's of type 1 (LITE's note-on seeks before any take), else (an update) the mode 1 voices with a seek pending.
// And before an update one VariPhrase voice in four given a seek pending, its level 0, so that it re-seeks: its band
// players' seeks are the only ones still going out by address (through the link's seek), and nothing else here set one
// until v330 (VariPhrase's settings were hooked).  From v330 (toneSteerSeeks until then) before a note-on of a
// VariPhrase block and an update of a VariPhrase voice, its update's words drawn finite (oscSteerVariWords)
void toneSteerOscs(ToneSides& s, int i, bool noteOn);
// v328: the voice whose records tone i's OSC's LITE settings read: the one its head names when that is another pool
// tone's LITE voice holding a loop record's address, as a take leaves it (counted), else its own, as v324's steer made it
uint32_t toneSettingsVoice(ToneSides& s, int i, int osc);
// v328: tone i's OSC whose LITE voice's head a take left on another tone's, −1 none
int toneTakenOsc(ToneSides& s, int i);
void toneOscBeforeNoteOn(ToneSides& s, int i, std::mt19937& rng);
void toneHoldOscVibratos(ToneSides& s, int i, int32_t n);
std::string toneFeedMic(ToneSides& s, int i, std::mt19937& rng);
// v322: before a render, tone i's voices whose render is one of the voice's own given the words it needs
// (oscSteerRenders), from the renders' own generator: the EXT IN player's level and target, from v323 Vari off's words
// on the case's wave (`toneSteerExtIn` until v323)
void toneSteerRenders(ToneSides& s, int i);
// Before a getter: a VariPhrase voice's +0x168, and from v321 half the time a LITE's or VariPhrase voice's positions
// (+0x194, +0x24c, +0x270), which their players' routines write (hooked here, writing none), drawn on both sides, so
// that the getters tell the voices apart
void toneDrawOscReads(ToneSides& s, int i, std::mt19937& rng);
void toneDrawOscSettings(ToneSides& s, int i, uint32_t osc, std::mt19937& rng);
void toneDrawOscRamp(Vc2ToneInput& in, std::mt19937& rng);
std::string toneOscSetUp(ToneSides& s);
std::string toneOscCase(ToneSides& s, std::mt19937& rng);

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
// An OSC's settings on tone i's OSC n, already prepared (Vc2ToneMethodCheck.cpp; v328: a take's note-on runs it too,
// `readsLoop`, on a type whose way reads the loop record)
std::string toneOscSettingsOf(ToneSides& s, int i, uint32_t n, std::mt19937& rng, bool readsLoop);
// The render event on tone i, already prepared (Vc2ToneEvents.cpp; v321: a COSM's new block's setter runs it too)
std::string toneRenderOf(ToneSides& s, int i, std::mt19937& rng);
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
