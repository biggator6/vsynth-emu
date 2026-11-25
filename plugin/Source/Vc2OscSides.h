#pragma once
// xt_vc2osc's two sides and what its events share (v230): rom.4's interpreter and its log, the port's voices, parts and
// shared draws, the shared routines (rom.4's own), and the counts.  The draws a note-on's block, the settings, the ramps'
// words and the mic ring take are Vc2OscDraws.h's from v319, shared with xt_vc2tone.  XT_VC2OSC_MAIN only.
#include "Vc2OscCheck.h"
#include "Vc2OscDraws.h"
#include "Vc2RomShared.h"
#include <functional>
#include <span>

struct OscCounts {
    long events = 0, noteOns = 0, plain = 0, sampled = 0, updates = 0, takes = 0, renders = 0, settings = 0;
    long getters = 0, gliding = 0;   // v331: the event lookups went; v335: the seeks
    long variDrawn = 0;              // v330: VariPhrase's words drawn finite (v335: its players' set-ups went)
    long variFormants = 0;           // v339: its formant's base, depths and factor drawn where its target isn't finite
    long variByEvents = 0;                 // v331: its note-ons by events, their table steered
    long carrierStarts = 0, carrierBlocks = 0, carrierSettings = 0, kind6Starts = 0, kind6Blocks = 0;   // v266
    long voicedSounding = 0, voicedNotFinite = 0;   // v266: those blocks with a sample that isn't 0, or isn't finite
    long extInBlocks = 0, variOffBlocks = 0;        // v322, v323: the renders through the EXT IN player's, Vari off's
    long headsBlocks = 0;                           // v326: and through LITE's heads'
    long modeOneBlocks = 0;                         // v327: and through kind 1's mode 1's
    long bandBlocks = 0;                            // v332: and through the band players', steered to a state
    long soloBlocks = 0;                            // v334: and through SOLO's, the same
    long jumpBlocks = 0;                            // v335: and through the period-jump player's
    long modeOneSetUps = 0, setUpsInSamples = 0;    // v328: its set-ups, and those with the range in samples
    long modeOneSeeks = 0;                           // v329: the events steered for its seek
    long liteUpdates = 0, liteSettings = 0;         // v324: LITE's update and settings on a type with a way
};

struct OscSides {
    Vc2Fw& fw;
    std::array<Vc2OscVoice, kOscVoices>& voices;
    std::vector<OscPortParts>& parts;
    OscShared& shared;
    std::vector<OscCall>& rom;
    const RomShared& romShared;
    OscCounts n {};
};

// An event on voice `voice` of both sides, after the draws: what differs, or nothing
using OscEvent = std::function<std::string(OscSides&, std::mt19937&, int voice)>;
std::span<const OscEvent> oscEvents();
std::string oscOnGetter(OscSides& s, std::mt19937& rng, int voice);       // a getter or a target (Vc2OscGetters.cpp)
std::string oscOnKindNoteOn(OscSides& s, std::mt19937& rng, int voice);   // 0x893af0b0 on its own (the same)
// The players' own draws (v238): kind 1 or 2, the wave's type 0…9 and, three times in four, the block's Vari off;
// VariPhrase's path is a kind, a type and a bit at once, too rare among the other draws (the same file)
void oscSteerPlayers(OscSides& s, std::mt19937& rng, int voice);
// The block's three record pointers on a voice, each a record's (Vc2OscCheck.cpp)
void oscDrawRecordWords(Vc2OscVoice& o, std::mt19937& rng);
// The voices' own words and tails against rom.4's memory, then the calls; empty when nothing differs
std::string oscDifference(OscSides& s);
// v266 (Vc2OscKindsCheck.cpp): the carrier and kind 6 on the voice.  Once, the static initializers; a case's start, the
// ring reset and both voices' two parts started as a note starts them; a block of the mic into the ring with the
// tracker's period and the unvoiced decision; what differs in the two parts and the ring's start word
std::string oscKindsSetUp(OscSides& s);
std::string oscKindsCase(OscSides& s, std::mt19937& rng);
std::string oscMicBlock(OscSides& s, std::mt19937& rng);
std::string oscPartsDifference(OscSides& s);
void oscCountBlock(OscSides& s, uint32_t kind);   // a rendered block of kind 5 or 6: does it sound, is it finite

enum class OscAnswer : uint8_t { none, r0, fr0 };

// rom.4's `routine` on `args`, then the port's `mine()` (giving its answer's bits), then what differs, the answers
// compared as `how` says; a difference names the routine (v248)
template <class Mine>
std::string oscBothSides(OscSides& s, uint32_t routine, std::initializer_list<uint32_t> args, OscAnswer how,
                         Mine mine) {
    char at[24];
    std::snprintf(at, sizeof at, "%08x: ", routine);
    if (!s.fw.call(routine, args)) return at + ("rom.4's routine faults: " + s.fw.cpu.error());
    const uint32_t theirs = how == OscAnswer::fr0 ? s.fw.cpu.fb[0][0] : s.fw.cpu.r[0];
    const uint32_t ours = mine();
    const std::string what = oscDifference(s);
    if (!what.empty()) return at + what;
    if (how == OscAnswer::none || theirs == ours) return what;
    char t[96];
    std::snprintf(t, sizeof t, "%sthe answer: %08x, against %08x", at, ours, theirs);
    return t;
}
// The kinds' note-on event's draws for kind 3 on voice `v` (v242; here from v246, Vc2OscAnalogDraws.cpp): its block
// words, a pitch near the root, the object's words, the sub oscillator's steers, LA's and NOISE's
void oscSteerAnalog(OscSides& s, std::mt19937& rng, int v);
// v248: voice `v` made kind 3 and its words drawn so (the install's and update's events, Vc2OscEvents.cpp)
void oscMakeAnalog(OscSides& s, std::mt19937& rng, int v);
// The port's voice `v`'s own words and tail written into rom.4's memory, so both sides start an event the same
void oscWriteVoice(OscSides& s, int v);
// v329 (Vc2OscEvents.cpp): before an event that `seeks` through mode 1's seek, voice `v`'s mode 1 words drawn
// (oscSteerModeOneSeek), counted, and the voice written
void oscSteerSeek(OscSides& s, int v, bool seeks);
// v327 (Vc2OscModeOneCheck.cpp): the mode 1 part, after the hooks' install and before the voice part
void oscModeOnePart(OscSides& s, const Vc2Run& run, Vc2Tally& tally);
// v329 (the same, v333 its own): a seek's word by a way drawn from eight, about the events at `positions` of `count`:
// in samples about an event's position half the time, on one a quarter, below 0 or past the last an eighth each; by
// events a number up to two past the last, one time in eight 0x8000 up, one in four with high bits besides
int32_t oscSeekWord(bool inSamples, std::span<const int32_t> positions, int count, std::mt19937& rng);
// v332 (Vc2OscBandsCheck.cpp): the band part, the band players' renders on voice 0, before any hook; from v333 their
// set-ups and seek too; v334: SOLO's part the same, its render, set-ups and seek; v335: the period-jump player's, each
// a family's part (`family`)
void oscPlayersPart(OscSides& s, const Vc2Run& run, Vc2Tally& tally, OscFamily family);
// v267 (Vc2OscVibratoCheck.cpp): the vibrato against rom.4's own, before any hook; its words as a note leaves them
void oscVibratoPart(Vc2Fw& fw, const Vc2Run& run, Vc2Tally& tally);
void oscDrawVibratoWords(Vc2VibratoWords& w, std::mt19937& rng);
// The ramp part (v236, Vc2RampCheck.cpp): the pitch ramp against rom.4's own, before the voice part hooks anything;
// its cases counted in `tally`, its counts printed
void oscRampPart(Vc2Fw& fw, const RomShared& shared, const Vc2Run& run, Vc2Tally& tally);
// The Vari off part (v261, Vc2VariOffCheck.cpp): its two renders against rom.4's, before any hook, the same way
void oscVariOffPart(Vc2Fw& fw, const Vc2Run& run, Vc2Tally& tally);
// A voice's ramps as glides leave them, each of the type its index is built with (the same file)
void oscDrawBuiltRamps(std::array<Vc2PitchRamp, Vc2OscVoice::kRamps>& ramps, std::mt19937& rng);
