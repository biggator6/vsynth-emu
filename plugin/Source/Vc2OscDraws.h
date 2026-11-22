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
// v330: before VariPhrase's update or note-on, the five words its update steps (+0x1cc…+0x1dc) and the level's target
// (+0x6c) drawn again moderate when not finite, as a note leaves them: the step takes each from its target by FSUB,
// whose NaN's sign is the host's (xtf::sub; a case's noise had one); how many were drawn
long oscSteerVariWords(Vc2OscVoice& o, std::mt19937& rng);

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
// Vari off's object words the renders read, by their offsets in the object (the voice's +0x15c on), as
// Vc2OscVariOffRender.cpp names them: the pitch, the gain's target and the gain; the fraction, the position, the samples'
// address, the last sample, the loop's first, last and length, its rate factor and mode; the stereo render's fold, the
// fade at the last sample, the position a turning loop runs on (v261's, Vc2VariOffCheck.cpp's until v323)
namespace OscVariOff {
inline constexpr uint32_t kPitchAt = 0x70, kTargetAt = 0x80, kGainAt = 0x84, kFracAt = 0xec, kPosAt = 0xf0;
inline constexpr uint32_t kSamplesAt = 0xf4, kLastAt = 0xf8, kStartAt = 0xfc, kEndAt = 0x100, kSpanAt = 0x104;
inline constexpr uint32_t kRateAt = 0x108, kModeAt = 0x10c, kFoldAt = 0x124, kFadeAt = 0x128, kTurnAt = 0x12c;
inline constexpr uint32_t kFirst = 0x70, kBytes = 0xc0;   // the object's words drawn: +0x70…+0x130
} // namespace OscVariOff
// v261's draws (Vc2VariOffCheck.cpp's until v323, the voice part's too): a wave's halfwords, one in 16 at full scale;
// and Vari off's words on a voice's object, on a wave at `waveAt`: noise under them all, a loop of 1…600 frames from
// frame 1024…2047, a last sample past its end three times in four (else inside it, where the hold is), the two
// positions about the loop and its way back, a mode 0…2, a fold one time in three, a pitch above 0.  Not drawn: a
// loop's length of 0 or less (rom.4's wrap never ends), a mode past 2 (rom.4 jumps into its table), a pitch of 0 or less
void oscDrawHalfwords(std::span<int16_t> wave, std::mt19937& rng);
void oscDrawVariOffWords(Vc2OscVoice& o, std::mt19937& rng, uint32_t waveAt);
// v326: LITE's heads' words on a voice's object as a note gives them, on the wave at `waveAt` (Vc2OscPartDraws.cpp's
// OscHeads names them): a range and both loops, each 64 frames or more, from frame 2500 to under 5000, inside a wave
// of 8192 frames, mono or stereo (a head reads only within the range); the walk's position about them, the heads
// within a period of it, a period of 50…400 frames, a pitch and formant of 0.5…2 (a head moves at most 1600 frames
// before its window splices it back); the loops' modes 0…2, their rates 0.5…2; the walk ±0.1…3 and the beat's factor
// 0…2, Tempo Sync and Time Trip off three times in four; the gain and its target 0…0.5 (two heads at full scale then
// give full scale), the filter's rate 0…0.05; the loop events' record one of the drawn.  Not drawn: a loop of no
// length (rom.4's wrap never ends), positions far from the loops (its wrap's sum would stand still)
void oscDrawHeadsWords(Vc2OscVoice& o, std::mt19937& rng, uint32_t waveAt);
// [FW] the renders the voice has as its own (v322, v323, v326): the EXT IN player's two, Vari off's two, LITE's heads'
// two
inline constexpr std::array<uint32_t, 2> kOscExtInRenders { 0x893c62e0, 0x893c61e0 };
inline constexpr std::array<uint32_t, 2> kOscVariOffRenders { 0x893c63c0, 0x893c66e0 };
inline constexpr std::array<uint32_t, 2> kOscHeadsRenders { 0x893c6e80, 0x893c6e8c };
inline constexpr std::array<uint32_t, 1> kOscModeOneRenders { 0x893c9f60 };   // v327: mono and stereo alike
// Whether a voice's render is one of `renders` (its kind renders by +0xa44, and that names one); before a render, a
// voice's words as the render needs them (`oscPlaysExtIn` and `oscSteerExtIn` until v323):
// - through the EXT IN player (v322), its level (+0x1e0) and target (+0x1dc) within ±1 as a note gives them (the
//   targets' level and gain), each drawn again 0…1 when not: the render takes the level from the target by xtf::sub,
//   whose NaN's sign is FSUB's negated; and its output, at most full scale so, is what a tone's delay mix moves its
//   delay by (v269: past full scale it reads past its lines);
// - through Vari off (v323), the object's words drawn as v261's are, on the wave at kOscWaveAt, so its reads stay in it
//   and its wrap ends;
// - through LITE's heads (v326), the object's words drawn by oscDrawHeadsWords, on the same wave;
// - through kind 1's mode 1 (v327), by oscDrawModeOneWords, about the case's events' table and on the same wave.
// From the renders' generator, the shared draws' (`renders`, and from v327 the records' events' table)
bool oscRendersBy(const Vc2OscVoice& o, std::span<const uint32_t> renders);
void oscSteerRenders(Vc2OscVoice& o, OscShared& shared);
// v327: the beat's block's words, as LITE's heads' and mode 1's draws give them (oscDrawHeadsWords' lines, moved): the
// walk ±0.1…3, the factor and its integral 0…2, the beat's position about the loop events', Tempo Sync and Time Trip
// off three times in four, byte 3 any byte half the time
void oscDrawBeatWords(Vc2OscVoice& o, std::mt19937& rng);
// v327 (Vc2OscWalkDraws.cpp): mode 1's object's words as a note gives them, about the case's events' table: the range
// and the position about its events, the current event and those beside it, the step's words (the beat's), the
// pitch and rates, the gain's three (the voice's +0xdc…), the flags (stereo, the loops), the walk's and events' bits;
// then one of four steers (Vc2OscWalkDraws.cpp's steerWalk): bouncing in either loop with the event ahead at the
// loop's last point, a sample's step landing on the event ahead, at a loop point ahead or on it, or none
void oscDrawModeOneWords(Vc2OscVoice& o, std::mt19937& rng, const OscRecords& r, uint32_t waveAt);
// v328 (Vc2OscWalkDraws.cpp): before an install on a LITE voice of type 1, whose set-up is mode 1's: the record words
// a note-on's block gives (the case's events' table, the wave, a header, a loop record, a +0xe0 record), the head at
// the voice's own +0x108 (`voiceAt`), byte 0's frame the table's three times in four and the range in samples half the
// time, the range's events (past the last now and then) and samples about the table's, the walk either way; from the
// renders' generator.  And a note-on's block of a LITE wave of type 1 given those record words (xt_vc2tone's)
void oscSteerModeOneSetUp(Vc2OscVoice& o, OscShared& shared, uint32_t voiceAt);
void oscModeOneBlock(uint32_t* block, std::mt19937& rng);
// v329 (Vc2OscWalkDraws.cpp): whether a voice, or a note-on's block, is LITE's of type 1, mode 1's; and before an event
// that `seeks` through mode 1's seek (0x893cc830), which walks the events from the object's current one, mode 1's
// words drawn by oscDrawModeOneWords about the case's table, from the renders' generator: 1 if drawn
bool oscIsModeOne(const Vc2OscVoice& o);
bool oscBlockIsModeOne(const uint32_t* block);
long oscSteerModeOneSeek(Vc2OscVoice& o, OscShared& shared, bool seeks);
// v323: a case's wave, from the renders' generator, into rom.4's memory at kOscWaveAt and the port's parts' both
void oscDrawWave(Vc2Fw& fw, OscShared& shared);
// v327 (Vc2OscWalkDraws.cpp), at the wave's draw: a case's events' table at kOscEventsAt, 4…12 events of a stride of
// 4, 6 or 8 half words from a first of 0 or 2 (even, as an entry's position is a MOV.L, which must be aligned), their
// positions rising through frames 2500…4900 about the loop events', their kinds a mark, a return or a turn three times
// in four; the region about it zeros, into rom.4's memory and the records' events both
void oscDrawEventTable(Vc2Fw& fw, OscShared& shared);
// v324: before a settings on a LITE voice (kind 1), the head's address (+0x1a0) the voice's own +0x108's at `voiceAt`,
// as its note-on leaves it (LITE's settings read the loop record through it, whatever the type), and the settings'
// type word (+0xb0) 0…2 half the time, the types with a way, and each of its four switches (+0x34…+0x40 from there) 0
// or 1 half the time, as a converter gives them; whether it is LITE's and its type has one
bool oscSteerLiteSettings(Vc2OscVoice& o, std::span<uint32_t> settings, uint32_t voiceAt, std::mt19937& rng);
std::string oscRingSetUp(const OscRingSides& r);
std::string oscRingCase(const OscRingSides& r, std::mt19937& rng);
std::string oscRingBlock(const OscRingSides& r, std::mt19937& rng);
std::string oscRingWordsDifference(const OscRingSides& r);
