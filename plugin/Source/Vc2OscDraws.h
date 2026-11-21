#pragma once
// What an OSC voice is given, drawn as its converters could give it (v230…v267, xt_vc2osc's), shared with xt_vc2tone
// from v319, whose tones' real voices take the same: a float by how wide it may be; a note-on's block (its floats, the
// analog's words, kinds 5's and 6's as a note gives them, the vibrato's block); the settings' block; the analog's object
// words; the vibrato's fade held for a count; the ramps' six words; and the mic ring with its globals, periods and
// unvoiced decision, set up and fed on both sides.  Moved, not copied: xt_vc2osc draws as it did, its lines as they
// were.  Vc2OscDraws.cpp (the blocks) and Vc2OscPartDraws.cpp (the vibrato, the ramps, the ring).
// XT_VC2OSC_MAIN or XT_VC2TONE_MAIN only.
#include "Vc2CheckRig.h"
#include "Vc2OscCheck.h"
#include <span>
#include <string>

inline constexpr uint32_t kOscBlockByte = 0x58;      // [FW] the voice's byte the block's +4 lands on
inline constexpr uint32_t kOscRingAt = 0x8a0f31cc;   // [FW] the mic ring's region, Vc2MicRingWords

// A float drawn evenly in [lo, hi) (v267: kinds 5 and 6's draws, and the vibrato's)
inline float oscWithin(std::mt19937& rng, float lo, float hi) { return lo + (hi - lo) * vc2Unit(rng); }

// How wide a drawn float may be: anything (NaNs, infinities, ±3e38 among the edges), no NaN, or moderate (0, ±1 and to
// ±10000: what reaches rom.4's exp2, which reads outside its tables, and faults, from an argument of 2^31 or a NaN),
// or about 2^31 / 10000 half the time and else no NaN (v238: a word VariPhrase's note-on takes × 10000 through 2^31)
enum class OscRange : uint8_t { any, noNaN, moderate, aboutTwo31 };
// A float drawn for a word the voice compares or adds: its usual range most often, its edges now and then
float oscDrawFloat(std::mt19937& rng, OscRange range = OscRange::any);
// A sample position's float: 2^31 and 2^32 themselves and either side, around them, or any but a NaN
float oscDrawPosition(std::mt19937& rng);
// The block's float words (a note-on's or the settings') that the targets compute with, drawn as floats rather than
// left as noise: those that reach exp2 moderate (the pitch, the formant, the pitch envelope's depth, the formant
// envelope's and LFO's, the vibrato's on the pitch), the others any.  Noise would hand the targets NaNs of random
// payloads, and which of two NaNs a product keeps is the host's (the interpreter's F(n) * F(m) as compiled), not rom.4's.
// Each word by its voice offset, and its range
struct OscDrawnFloat {
    uint32_t at;
    OscRange range;
};
std::span<const OscDrawnFloat> oscBlockFloatWords();
void oscBlockFloats(std::span<uint32_t> words, std::mt19937& rng);
// A note-on's block: noise, its kind 0…7 (past 7 rom.4 reads from address 0), the wave's type, the depth, the records
// (oscHeaderAt's, oscLoopAt's, oscPairAt's), the analog's words on kind 3, kinds 5's and 6's on theirs, the vibrato's
std::array<uint32_t, kOscBlockWords> oscDrawNoteBlock(std::mt19937& rng);
// The settings' block (0x893ae780's): noise, its floats, its byte +4 the voice's `held` half the time, kinds 5's and 6's
// words as a note's (the carrier's levels among them), the vibrato's rate and biases
void oscDrawSettingsBlock(std::span<uint32_t> settings, std::mt19937& rng, uint8_t held);
// v242: the analog note-on's block words (kind 3), each the voice's offset and its bits, as its converter could give
// them: the code 0…10 most often, past now and then; the sub oscillator's switch mostly off, never a NaN (FTRC reads
// it); the Tone KF's value within its table; the Tone, Fat, the Pulse Width, their depths, Impact and the Slope moderate
// (NOISE's hold takes FTRC of a sum, and two NaNs in a product are the host's); from v246 the sub level (+0x13c, the
// shape's) moderate too, and with the sub on and an LA code Fat's raw sum within ±120 (2^sum finite: ∞ × a phase of 0
// would reach xtf::sub)
inline constexpr size_t kOscAnalogWords = 13;
std::array<std::pair<uint32_t, uint32_t>, kOscAnalogWords> oscAnalogWords(std::mt19937& rng);
// v244: the object's words the analog's sub oscillator's note-on reads, drawn moderate on the voice before a kind-3
// note-on: noise or another kind's integers there could be NaNs, which reach xtf::sub; from v246 its shape (0x893c4020)
// writes some by the code, the others as drawn
void oscAnalogObject(Vc2OscVoice& o, std::mt19937& rng);
// Kinds 5 and 6 (v266), whose inputs are drawn as a note gives them: on a voice of those kinds, and in a note-on's
// block (words from the voice's `below`)
bool oscIsVoiced(uint32_t kind);
void oscDrawVoicedVoice(Vc2OscVoice& o, std::mt19937& rng);
void oscDrawVoicedWords(std::span<uint32_t> words, std::mt19937& rng, uint32_t below);

// The vibrato's block as the converter could write it (v267; waves 0 and 1 alone: past them rom.4 reads what lies after
// its table): alone, into a note-on's or the settings' words, or onto a voice's; and its fade's rates held to 1 over
// the count `n`, as past that the fade runs away to infinities whose NaNs are the host's
Vc2VibratoBlock oscVibratoBlockDrawn(std::mt19937& rng);
void oscDrawVibratoBlock(std::span<uint32_t> words, std::mt19937& rng);
void oscDrawVibratoVoice(Vc2OscVoice& o, std::mt19937& rng);
void oscHoldVibrato(Vc2VibratoWords& w, int32_t n);
// The ramps' (v236): a switch on half the time, else off or any word; the six words a ramp's start or legato update is
// given
uint32_t oscDrawOnSwitch(std::mt19937& rng);
Vc2RampSettings oscDrawRampSettings(std::mt19937& rng);

// The mic ring on both sides (v266): rom.4's memory and the port's ring, the unvoiced decision kind 6 reads and the OS's
// millisecond word a carrier's init reads.  Once, the carriers' rate and kind 6's globals' setter; a case's start,
// Unvoice Level and the clock drawn, the ring reset and written whole, so that a voice's reads 512 samples behind the
// write point find the mic; a block of the mic into it with the tracker's period and the unvoiced decision for it; what
// differs in the ring and kind 6's globals
struct OscRingSides {
    Vc2Fw& fw;
    Vc2MicRing& ring;
    std::span<float> unvoiced;
    int32_t& clock;
};
// The hooks' answers before an event, xt_vc2osc's and xt_vc2tone's (v321, the two checks' copies made one): the OS's
// clock, the input stage's peak (kind 5's update's Dynamics: 0 now and then, else a level, into rom.4's word
// 0x899d9a5c too), what a render answers, the flags a seek leaves, the event lookup's positions and its second word;
// none taken yet on either side, and the port's log of calls cleared; from v322 the effect chip's frames the EXT IN
// player plays, from the shared generator of their own, into rom.4's memory with the part's pointer to them
void oscDrawAnswers(Vc2Fw& fw, OscShared& shared, std::mt19937& rng);
// v322: whether a voice's render is the EXT IN player's (its kind renders by +0xa44, and that names one of the two);
// before a render on such a voice, its level (+0x1e0) and target (+0x1dc) within
// ±1 as a note gives them (the targets' level and gain), each drawn again 0…1 when not: the render takes the level from
// the target by xtf::sub, whose NaN's sign is FSUB's negated; and its output, at most full scale so, is what a tone's
// delay mix moves its delay by (v269: past full scale it reads past its lines)
bool oscPlaysExtIn(const Vc2OscVoice& o);
void oscSteerExtIn(Vc2OscVoice& o, std::mt19937& rng);
std::string oscRingSetUp(const OscRingSides& r);
std::string oscRingCase(const OscRingSides& r, std::mt19937& rng);
std::string oscRingBlock(const OscRingSides& r, std::mt19937& rng);
std::string oscRingWordsDifference(const OscRingSides& r);
