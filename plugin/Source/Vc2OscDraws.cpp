// The draws of what an OSC voice is given, its blocks: see Vc2OscDraws.h (v319; xt_vc2osc's, moved: the floats from
// Vc2OscCheck.cpp, a note-on's block and the settings' from Vc2OscEvents.cpp, the analog's from Vc2OscAnalogDraws.cpp,
// kinds 5's and 6's from Vc2OscKindsCheck.cpp).  XT_VC2OSC_MAIN or XT_VC2TONE_MAIN only.
#if defined(XT_VC2OSC_MAIN) || defined(XT_VC2TONE_MAIN)
#include "Vc2OscDraws.h"
#include <algorithm>
#include <bit>
#include <cmath>

namespace {
constexpr float kTwo31 = 2147483648.0f;
// [FW] the block's floats the routines compute with (each the block's word 0x54 below), each drawn as wide as it may
// be: those that reach exp2 through the targets moderate (the pitch, the formant, the pitch envelope's depth, the
// formant envelope's and LFO's, the vibrato's on the pitch, and where the pitch ramp starts from, +0xbc, v236: the
// ramp's value is a term of the pitch); VariPhrase's note-on's +0xb4 about 2^31 / 10000 and no NaN (v238: × 10000
// through FTRC by way of 2^31, and FTRC gives a NaN otherwise than the interpreter); the others any
constexpr std::array<OscDrawnFloat, 16> kFloats { {
    { 0x5c, OscRange::moderate }, { 0x68, OscRange::moderate }, { 0x70, OscRange::moderate },
    { 0x80, OscRange::moderate }, { 0xa0, OscRange::moderate }, { 0xfc, OscRange::moderate },
    { 0xbc, OscRange::moderate }, { 0xb4, OscRange::aboutTwo31 }, { 0x60, OscRange::any }, { 0x64, OscRange::any },
    { 0x6c, OscRange::any }, { 0x78, OscRange::any }, { 0x88, OscRange::any }, { 0x98, OscRange::any },
    { 0xb0, OscRange::any }, { 0x100, OscRange::any } } };
// [FW] the block's words a note-on's draw steers: its kind, its byte +4, the wave's type, the records (its floats by
// oscBlockFloats)
constexpr int kKindWord = 0, kByteWord = 1, kWaveWord = 0xb0 / 4, kHeaderWord = 0xd4 / 4;
constexpr int kLoopWord = 0xdc / 4, kFixedWord = 0xe4 / 4;
// [FW] v238: VariPhrase's note-on's +0xe0 record, and its range's start and where it seeks to (the block's +0xe8, +0xf0)
constexpr int kPairWord = 0xe0 / 4, kFromWord = 0xe8 / 4, kSeekToWord = 0xf0 / 4;
constexpr uint32_t kFixedBit = 0x1000;   // the byte +0x138's 0x10, in its word
constexpr int kKinds = 8, kWaves = 10;
constexpr uint32_t kAnalog = 3, kCarrierKind = 5, kKind6Kind = 6;   // [FW]
// [FW] the voice's words the targets give P from: the ratio's scale, the gain's two factors, the vibrato's depth on them,
// kind 6's level, the formant's base and its envelope's and LFO's depths
constexpr uint32_t kRatioScale = 0x60, kLevel = 0x88, kGain = 0xb0, kLevelDepth = 0x100, kKind6Level = 0x6c;
constexpr uint32_t kFormantBase = 0x68, kFormantEnvelope = 0x80, kFormantLfo = 0xa0;
// [FW] P's levels the carrier's init and settings take: Saw Wave LPF and Level, Noise LPF, HPF and Level; from v321,
// as kind 5's update reads them, Dynamics Threshold and Sensitivity
constexpr std::array<uint32_t, 7> kCarrierLevels { 0x110, 0x114, 0x118, 0x11c, 0x120, 0x128, 0x12c };
// [FW] v321: P's terms kind 5's update reads, as its converter gives them: Growl Level (0…1) and Saw-Noise Mix Balance
// (0…2), each with its envelope's and LFO's depths
constexpr uint32_t kGrowl = 0x134, kBalance = 0x148;
constexpr std::array<uint32_t, 4> kTermDepths { 0x138, 0x13c, 0x14c, 0x150 };
constexpr uint32_t kSettingsWordsBelow = 0x54;   // [FW] a voice's word lies this far above its place in the settings
constexpr float kFormantDepth = 0.001f;          // with an output of up to ±1000, an octave of the formant

using AnalogWords = std::array<std::pair<uint32_t, uint32_t>, kOscAnalogWords>;

void setWord(AnalogWords& words, uint32_t at, float x) {
    const auto word = std::ranges::find(words, at, &std::pair<uint32_t, uint32_t>::first);
    word->second = std::bit_cast<uint32_t>(x);
}

// v246: with the sub oscillator on and an LA code, Fat (+0x108) within ±40 and its envelope's and LFO's depths
// (+0x10c, +0x114) within ±0.04, the bound outputs being at most 1000: its raw sum within ±120 octaves.  rom.4's powf
// gives ∞ from 128 on, and ∞ × a phase of 0 is a NaN that LA's end (0x893c3cd0) subtracts, where xtf::sub's NaN sign
// is FSUB's negated.  The other codes take Fat's ∞ through products alone, so they keep the moderate draws
void steerSubFat(AnalogWords& words, std::mt19937& rng) {
    constexpr float kFatMost = 40.0f, kDepthMost = 0.04f;
    setWord(words, 0x108, kFatMost * (2.0f * vc2Unit(rng) - 1.0f));
    for (uint32_t at : { 0x10cu, 0x114u }) setWord(words, at, kDepthMost * (2.0f * vc2Unit(rng) - 1.0f));
}

// Kinds 5's and 6's words as a note gives them, each put at its voice offset
template <class Put>
void drawNoteWords(Put put, std::mt19937& rng) {
    put(kRatioScale, oscWithin(rng,0.5f, 2.0f));
    put(kLevel, vc2Unit(rng));
    put(kGain, vc2Unit(rng));
    put(kLevelDepth, oscWithin(rng,-1.0f, 1.0f));
    put(kKind6Level, vc2Unit(rng));
    put(kFormantBase, oscWithin(rng,-1.0f, 1.0f));
    put(kFormantEnvelope, oscWithin(rng,-kFormantDepth, kFormantDepth));
    put(kFormantLfo, oscWithin(rng,-kFormantDepth, kFormantDepth));
    for (uint32_t at : kCarrierLevels) put(at, vc2DrawLevel(rng));
    put(kGrowl, vc2Unit(rng));
    put(kBalance, 2.0f * vc2Unit(rng));
    for (uint32_t at : kTermDepths) put(at, oscWithin(rng, -1.0f, 1.0f));
}
} // namespace

float oscDrawPosition(std::mt19937& rng) {
    const std::array<float, 4> edges { kTwo31, std::nextafter(kTwo31, 0.0f), 2.0f * kTwo31,
                                       std::nextafter(2.0f * kTwo31, 0.0f) };
    if (rng() % 2 == 0) return edges[rng() % edges.size()];
    return rng() % 2 ? kTwo31 * (0.5f + 1.5f * vc2Unit(rng)) : oscDrawFloat(rng, OscRange::noNaN);
}

float oscDrawFloat(std::mt19937& rng, OscRange range) {
    constexpr float kTimes = 10000.0f;   // [FW] 0x893af53c: VariPhrase's note-on takes the word × it through 2^31
    if (range == OscRange::aboutTwo31 && rng() % 2) return oscDrawPosition(rng) / kTimes;
    constexpr std::array<float, 8> kEdges { 0.0f, -0.0f, 1.0f, -1.0f, 3e38f, -3e38f, INFINITY, -INFINITY };
    constexpr std::array<float, 4> kScales { 1.0f, 100.0f, 10000.0f, 0.001f };
    // moderate: 0, −0, ±1 and to ±1000, so that an output times its depth reaches exp2 below 2·10^6, where its tables'
    // reads stay in rom.4's memory (the pitch's still pass its ±6000)
    constexpr std::array<float, 4> kModerateScales { 1.0f, 100.0f, 1000.0f, 0.001f };
    constexpr size_t kModerateEdges = 4;
    const bool moderate = range == OscRange::moderate;
    if (rng() % 4 == 0) return kEdges[rng() % (moderate ? kModerateEdges : kEdges.size())];
    if (range == OscRange::any && rng() % 32 == 0) return NAN;   // aboutTwo31's other half as noNaN
    return (2.0f * vc2Unit(rng) - 1.0f) * (moderate ? kModerateScales : kScales)[rng() % kScales.size()];
}

std::span<const OscDrawnFloat> oscBlockFloatWords() { return kFloats; }

void oscBlockFloats(std::span<uint32_t> words, std::mt19937& rng) {
    for (const auto& [at, range] : kFloats) {
        words[(at - Vc2OscVoice::kOwn) / 4] = std::bit_cast<uint32_t>(oscDrawFloat(rng, range));
    }
}

std::array<uint32_t, kOscBlockWords> oscDrawNoteBlock(std::mt19937& rng) {
    std::array<uint32_t, kOscBlockWords> b {};
    for (uint32_t& w : b) w = (uint32_t)rng();
    oscBlockFloats(b, rng);   // the depth (+0x1c) among them
    b[kKindWord] = rng() % kKinds;
    b[kWaveWord] = rng() % 8 == 0 ? (uint32_t)rng() : rng() % kWaves;
    b[kHeaderWord] = oscHeaderAt((int)(rng() % kOscRecords));
    b[kLoopWord] = oscLoopAt((int)(rng() % kOscRecords));
    b[kPairWord] = oscPairAt((int)(rng() % kOscRecords));   // v238: VariPhrase's note-on reads its +0x2d0
    if (rng() % 2) b[kSeekToWord] = b[kFromWord];             // v238: no seek when the third is the first
    if (rng() % 2) b[kByteWord] = 0;
    if (rng() % 2) b[kFixedWord] &= ~kFixedBit;
    if (b[kKindWord] == kAnalog) {   // v242: the analog's block words, as its converter could give them
        for (const auto& [at, bits] : oscAnalogWords(rng)) b[(at - Vc2OscVoice::kOwn) / 4] = bits;
    }
    if (oscIsVoiced(b[kKindWord])) oscDrawVoicedWords(b, rng, Vc2OscVoice::kOwn);   // v266: as a note gives them
    oscDrawVibratoBlock(b, rng);                                                     // v267: the same
    return b;
}

void oscDrawSettingsBlock(std::span<uint32_t> settings, std::mt19937& rng, uint8_t held) {
    for (uint32_t& w : settings) w = (uint32_t)rng();
    oscBlockFloats(settings, rng);
    auto* bytes = reinterpret_cast<uint8_t*>(settings.data());
    if (rng() % 2) bytes[4] = held;                                 // the byte +4 as the voice holds it, half the time
    oscDrawVoicedWords(settings, rng, kSettingsWordsBelow);         // v266: the carrier's levels among them, as a note's
    oscDrawVibratoBlock(settings, rng);                             // v267: the vibrato's rate and biases
}

AnalogWords oscAnalogWords(std::mt19937& rng) {
    constexpr uint32_t kCodes = 11, kPast = 8, kToneKfs = 15;   // [FW] the codes 0…10; the Tone KF table's 15
    constexpr uint32_t kLaFirst = 9, kLaCodes = 2;              // [FW] LA-SQUARE's and LA-SAW's codes
    constexpr std::array<float, 6> kSubOn { 1.0f, 2.0f, -1.0f, 0.99f, 1e9f, -0.0f };
    uint32_t code = rng() % kPast == 0 ? kCodes + (uint32_t)rng() % kPast : (uint32_t)rng() % kCodes;
    // v246: the sub oscillator's switch drawn one time in two (one in four before), and with it on an LA code one time
    // in three, so that the shape's LA way is reached at 40 cases
    const float subOn = rng() % 2 == 0 ? kSubOn[rng() % kSubOn.size()] : 0.0f;
    if (std::trunc(subOn) != 0.0f && rng() % 3 == 0) code = kLaFirst + (uint32_t)rng() % kLaCodes;
    const float toneKf = (float)(rng() % kToneKfs) + 0.9f * vc2Unit(rng);
    // Impact as the converter gives it, Impact·4/40: its tenths 0…4, 1 (the default) one time in four; else moderate
    constexpr uint32_t kTenths = 41;
    const float tenths = (float)(rng() % kTenths) / 10.0f;
    const float impact = rng() % 4 == 0 ? 1.0f : rng() % 2 ? tenths : oscDrawFloat(rng, OscRange::moderate);
    AnalogWords words { { { 0x104, code }, { 0x138, std::bit_cast<uint32_t>(subOn) },
                          { 0x144, std::bit_cast<uint32_t>(toneKf) }, { 0x130, std::bit_cast<uint32_t>(impact) } } };
    constexpr std::array<uint32_t, 9> kModerate { 0x108, 0x10c, 0x114, 0x11c, 0x120, 0x124, 0x134, 0x140, 0x13c };
    for (size_t k = 0; k < kModerate.size(); ++k) {
        words[4 + k] = { kModerate[k], std::bit_cast<uint32_t>(oscDrawFloat(rng, OscRange::moderate)) };
    }
    if (code - kLaFirst < kLaCodes && std::trunc(subOn) != 0.0f) steerSubFat(words, rng);
    return words;
}

void oscAnalogObject(Vc2OscVoice& o, std::mt19937& rng) {
    // [FW] Fat's and the Pulse Width's targets, the phase, the second wave's Pulse Width target, the level's target,
    // RAMP's last segment, the two words nothing writes, LA's steepness targets, SQUARE's and JUNO's
    constexpr std::array<uint32_t, 12> kWords { 0x24c, 0x254, 0x260, 0x278, 0x28c, 0x294, 0x298, 0x2a0, 0x2b0, 0x2b8,
                                                0x2c0, 0x2c8 };
    for (uint32_t at : kWords) o.setF32(at, oscDrawFloat(rng, OscRange::moderate));
}

bool oscIsVoiced(uint32_t kind) { return kind == kCarrierKind || kind == kKind6Kind; }

void oscDrawVoicedVoice(Vc2OscVoice& o, std::mt19937& rng) {
    if (!oscIsVoiced(o.kind())) return;
    drawNoteWords([&](uint32_t at, float x) { o.setF32(at, x); }, rng);
    o.tail.rateRatio = oscWithin(rng,0.5f, 2.0f);
    o.tail.formantFactor = oscWithin(rng,0.5f, 2.0f);
}

void oscDrawVoicedWords(std::span<uint32_t> words, std::mt19937& rng, uint32_t below) {
    drawNoteWords([&](uint32_t at, float x) { words[(at - below) / 4] = std::bit_cast<uint32_t>(x); }, rng);
}
#endif
