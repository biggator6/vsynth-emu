#pragma once
// xt_vc2osc's two sides and what its events share (v230): rom.4's interpreter and its log, the port's voices, parts and
// shared draws, the shared routines (rom.4's own), and the counts.  XT_VC2OSC_MAIN only.
#include "Vc2OscCheck.h"
#include "Vc2RomShared.h"
#include <functional>
#include <span>

struct OscCounts {
    long events = 0, noteOns = 0, plain = 0, sampled = 0, updates = 0, takes = 0, renders = 0, settings = 0;
    long getters = 0, seeks = 0, kindCalls = 0, gliding = 0, lookups = 0, objectCalls = 0;
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
// The voices' own words and tails against rom.4's memory, then the calls; empty when nothing differs
std::string oscDifference(OscSides& s);

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
// How wide a drawn float may be: anything (NaNs, infinities, ±3e38 among the edges), no NaN, or moderate (0, ±1 and to
// ±10000: what reaches rom.4's exp2, which reads outside its tables, and faults, from an argument of 2^31 or a NaN),
// or about 2^31 / 10000 half the time and else no NaN (v238: a word VariPhrase's note-on takes × 10000 through 2^31)
enum class OscRange : uint8_t { any, noNaN, moderate, aboutTwo31 };
// A float drawn for a word the voice compares or adds: its usual range most often, its edges now and then
float oscDrawFloat(std::mt19937& rng, OscRange range = OscRange::any);
// The block's float words (a note-on's or the settings') that the targets compute with, drawn as floats rather than
// left as noise: those that reach exp2 moderate (the pitch, the formant, the pitch envelope's depth, the formant
// envelope's and LFO's, the vibrato's on the pitch), the others any.  Noise would hand the targets NaNs of random
// payloads, and which of two NaNs a product keeps is the host's (the interpreter's F(n) * F(m) as compiled), not rom.4's
void oscBlockFloats(std::span<uint32_t> words, std::mt19937& rng);
// v242: the analog note-on's block words (kind 3; Vc2OscAnalogDraws.cpp), each the voice's offset and its bits, as its
// converter could give them: the code 0…10 most often, past now and then; the sub oscillator's switch mostly off,
// never a NaN (FTRC reads it); the Tone KF's value within its table; the Tone, Fat, the Pulse Width, their depths,
// Impact and the Slope moderate (NOISE's hold takes FTRC of a sum, and two NaNs in a product are the host's); from
// v246 the sub level (+0x13c, the shape's) moderate too, and with the sub on and an LA code Fat's raw sum within ±120
// (2^sum finite: ∞ × a phase of 0 would reach xtf::sub)
inline constexpr size_t kOscAnalogWords = 13;
std::array<std::pair<uint32_t, uint32_t>, kOscAnalogWords> oscAnalogWords(std::mt19937& rng);
// v244: the object's words the analog's sub oscillator's note-on reads, drawn moderate on the voice before a kind-3
// note-on: noise or another kind's integers there could be NaNs, which reach xtf::sub (Vc2OscAnalogDraws.cpp); from
// v246 its shape (0x893c4020) writes some by the code, the others as drawn
void oscAnalogObject(Vc2OscVoice& o, std::mt19937& rng);
// v242 (here from v246, Vc2OscAnalogDraws.cpp): the kinds' note-on event's draws for kind 3 on voice `v`: its block
// words, a pitch near the root, the object's words, the sub oscillator's steers, LA's and NOISE's
void oscSteerAnalog(OscSides& s, std::mt19937& rng, int v);
// v248: voice `v` made kind 3 and its words drawn so (the install's and update's events, Vc2OscEvents.cpp)
void oscMakeAnalog(OscSides& s, std::mt19937& rng, int v);
// A sample position's float: 2^31 and 2^32 themselves and either side, around them, or any but a NaN
float oscDrawPosition(std::mt19937& rng);
// The port's voice `v`'s own words and tail written into rom.4's memory, so both sides start an event the same
void oscWriteVoice(OscSides& s, int v);
// The ramp part (v236, Vc2RampCheck.cpp): the pitch ramp against rom.4's own, before the voice part hooks anything;
// its cases counted in `tally`, its counts printed
void oscRampPart(Vc2Fw& fw, const RomShared& shared, const Vc2Run& run, Vc2Tally& tally);
// The Vari off part (v261, Vc2VariOffCheck.cpp): its two renders against rom.4's, before any hook, the same way
void oscVariOffPart(Vc2Fw& fw, const Vc2Run& run, Vc2Tally& tally);
// A voice's ramps as glides leave them, each of the type its index is built with (the same file)
void oscDrawBuiltRamps(std::array<Vc2PitchRamp, Vc2OscVoice::kRamps>& ramps, std::mt19937& rng);
// The six words a ramp's start or legato update is given (the same)
Vc2RampSettings oscDrawRampSettings(std::mt19937& rng);
