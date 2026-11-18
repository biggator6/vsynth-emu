// xt_vc2carrier's third part (v251): the OSC block's converter against rom.4's whole converters, the analog's (0x8939cf70)
// and, one conversion in four, the VOCODER's (0x8939db00), on the first parts' interpreter.  Each conversion draws an
// input (the header's OSC, key and velocity; the record's pitch, level and gain bytes and KFs, the waveform; the note's
// coarse, fine and second key; now and then any byte, halfword or word, which the loads' widths, signs and sums must
// take; the rest 0, the two pitch10s' words noise), the Matrix's two groups, which hooks on rom.4's four getters give,
// the pitch source's ratio, which a hook on 0x89394a40 gives in fr0, and M's bytes +0x182/+0x183 and the calibration's
// eleven rows, written into rom.4's memory; the block noise on both; rom.4's whole converter on the input and the
// block, the port's part on the same; the block's +0x00…+0xb0 and the input word for word (the VOCODER's block from
// +0xb0 is its own part's).  From v256 the analog's conversions draw its record's words too (Vc2OscConvertDraws.cpp),
// and the port's second part runs after the first, the block compared to +0xf4.  The velocity curve and powf are rom.4's
// own on both sides (RomShared).  XT_VC2CARRIER_FIRST and XT_VC2CARRIER_STOP as the first part's.  XT_VC2CARRIER_MAIN
// only.
#ifdef XT_VC2CARRIER_MAIN
#include "Vc2CarrierCheck.h"
#include "Vc2OscAnalogConvert.h"
#include "Vc2RomShared.h"
#include <bit>
#include <cstdio>
#include <cstring>

namespace {
constexpr uint32_t kAnalog = 0x8939cf70, kVocoder = 0x8939db00, kPitchSource = 0x89394a40;   // [FW]
constexpr uint32_t kCalibrationAt = 0x89e371e0 + 0x1020e, kBeamAt = 0x182;   // [FW] 0x8939c9a0's rows; M's bytes
constexpr uint32_t kInputAt = 0x8C080000, kBlockAt = 0x8C081000, kOscSumsAt = 0x8C082000;   // in Vc2Fw's RAM
constexpr uint32_t kLfoSumsAt = 0x8C082020, kMatrixAt = 0x8C083000;
constexpr int kInputBytes = 0x108, kBlockBytes = 0x108, kCompared = 0xb0, kConversions = 50;
constexpr int kAnalogCompared = kVc2OscAnalogWordsAt + (int)sizeof(Vc2OscAnalogWords);
constexpr int kMatrixWordAt = 0x100, kPitch10At = 0x94, kFineAt = 0x90;   // [FW] in the input
static_assert(sizeof(Vc2OscCalibration) == 4, "rom.4's layout, a calibration row's four bytes");

struct Counts {
    long conversions = 0, vocoder = 0, kfZero = 0, kfWhole = 0, rowsPast = 0;
};

// [FW] the record's bytes by the input's offset, and their ranges: Wave Gain (dB), Pitch, the pitch's LFO and envelope
// depths and the envelope's velocity curve, Level, its LFO depth and velocity curve; the second key
constexpr Vc2ByteDraw kBytes[] = {
    { 0x0a, -12, 12 }, { 0x0c, -63, 63 }, { 0x10, -63, 63 }, { 0x11, -63, 63 }, { 0x12, 0, 7 }, { 0x1c, 0, 127 },
    { 0x20, -63, 63 }, { 0x21, 0, 7 }, { 0x9c, 0, 127 },
};
// [FW] the halfwords: the velocity sens (pitch, level), Level KF, the note's coarse
constexpr Vc2ByteDraw kHalfwords[] = { { 0x14, -200, 200 }, { 0x22, -200, 200 }, { 0x1e, -2000, 2000 },
                                       { 0x88, -96, 96 } };
constexpr int kPitchKfAt = 0x0e, kWaveAt = 0x28;
constexpr Vc2LfoEdges kLfoEdges { { 0x10, 0x20 }, { 1, 4 } };   // [FW] the LFO's Pitch and Level depths and sums

// Pitch KF: 0 and 100% (1000) a quarter each, where rom.4 forms the sum without the product; else ±200%, or any
int drawPitchKf(std::mt19937& rng, Counts& n) {
    const uint32_t pick = rng() % 4;
    n.kfZero += pick == 0;
    n.kfWhole += pick == 1;
    constexpr int kKfs[2] = { 0, 1000 };
    return pick < 2 ? kKfs[pick] : vc2DrawHalf(rng, -2000, 2000);
}

// The note's fine, tenths of a cent: 0 a quarter of the time, mostly a bend's ±2000 cents, now and then any word
int32_t drawFine(std::mt19937& rng) {
    const uint32_t pick = rng() % 8;
    if (pick == 0) return (int32_t)rng();
    return pick < 3 ? 0 : (int32_t)(rng() % 40001) - 20000;
}

// An input: the header, the record's bytes and halfwords, the note's words, M's address; the rest 0, the two pitch10s'
// words noise, so a word the converter leaves stays as it was
void drawBlockInput(std::mt19937& rng, uint8_t* in, Counts& n) {
    std::memset(in, 0, kInputBytes);
    in[2] = (uint8_t)(rng() % 8 == 0 ? rng() : rng() % 2);
    in[3] = (uint8_t)vc2DrawByte(rng, 0, 127);
    in[4] = (uint8_t)vc2DrawByte(rng, 1, 127);
    for (const Vc2ByteDraw& b : kBytes) in[b.at] = (uint8_t)vc2DrawByte(rng, b.lo, b.hi);
    for (const Vc2ByteDraw& h : kHalfwords) vc2Put16(in + h.at, vc2DrawHalf(rng, h.lo, h.hi));
    vc2Put16(in + kPitchKfAt, drawPitchKf(rng, n));
    in[kWaveAt] = (uint8_t)vc2DrawByte(rng, 0, 10);
    const int32_t fine = drawFine(rng);
    std::memcpy(in + kFineAt, &fine, sizeof fine);
    for (int w = 0; w < 2; ++w) {
        const uint32_t noise = rng();
        std::memcpy(in + kPitch10At + 4 * w, &noise, sizeof noise);
    }
    std::memcpy(in + kMatrixWordAt, &kMatrixAt, sizeof kMatrixAt);
}

// What the converter reads besides the input: the groups (0 half the time, else a little; the pitch's sum half the
// time across ±13000, past ±63 once scaled; the LFO's within ±70, its edges now and then); M's two bytes (none half
// the time); the pitch source (1 half the time, as with Auto Note off); the calibration rows (bias points about the
// keyboard, KFs mostly small, Levels in range)
Vc2OscConvertIn drawWorld(std::mt19937& rng, const uint8_t* in) {
    Vc2OscConvertIn w {};
    for (int16_t& v : w.sums.osc) v = rng() % 2 == 0 ? 0 : (int16_t)vc2DrawHalf(rng, -200, 200);
    if (rng() % 2 == 0) w.sums.osc[0] = (int16_t)((int)(rng() % 26001) - 13000);
    vc2DrawLfoSums(rng, in, w.sums.lfo, 70, kLfoEdges);
    if (rng() % 2 == 0) {
        for (uint8_t& b : w.beam) b = (uint8_t)vc2DrawByte(rng, 0, 127);
    }
    w.pitchSource = rng() % 2 == 0 ? 1.0f : vc2Within(rng, 0.25f, 4.0f);
    for (Vc2OscCalibration& c : w.calibration) {
        c.bias = (int8_t)vc2DrawByte(rng, 24, 108);
        c.upper = (int8_t)vc2DrawByte(rng, -40, 40);
        c.lower = (int8_t)vc2DrawByte(rng, -40, 40);
        c.level = (int8_t)vc2DrawByte(rng, 0, 127);
    }
    return w;
}

// The drawn world into rom.4's memory, the pitch source's answer into the hook's word
void writeWorld(Vc2Fw& fw, const Vc2OscConvertIn& w, float& source) {
    fw.cpu.write(kOscSumsAt, w.sums.osc, sizeof w.sums.osc);
    fw.cpu.write(kLfoSumsAt, w.sums.lfo, sizeof w.sums.lfo);
    fw.cpu.write(kMatrixAt + kBeamAt, w.beam.data(), w.beam.size());
    fw.cpu.write(kCalibrationAt, w.calibration.data(), sizeof w.calibration);
    vc2WriteAnalogWorld(fw, w, kMatrixAt);
    source = w.pitchSource;
}

// The generators and the counts a conversion draws from: v251's, and the analog's second part's of its own
struct Draws {
    std::mt19937 rng, analog;
    float source = 1.0f;
    Counts n {};
    Vc2OscAnalogCounts a {};
};

// One conversion on both: rom.4's converter of a kind drawn, the port's parts of that kind
std::string conversion(Vc2Fw& fw, Draws& d) {
    uint8_t in[kInputBytes], block[kBlockBytes];
    drawBlockInput(d.rng, in, d.n);
    Vc2OscConvertIn w = drawWorld(d.rng, in);
    for (int k = 0; k < kBlockBytes / 4; ++k) {
        const uint32_t noise = d.rng();
        std::memcpy(block + 4 * k, &noise, sizeof noise);
    }
    const bool vocoder = d.rng() % 4 == 0;
    d.n.vocoder += vocoder;
    if (!vocoder) vc2DrawAnalogRecord(d.analog, in, w, d.a);
    const int row = vocoder ? kVc2OscVocoderRow : vc2OscAnalogRow(in);
    d.n.rowsPast += row > kVc2OscCalibrationRows - 1;
    fw.cpu.write(kInputAt, in, sizeof in);
    fw.cpu.write(kBlockAt, block, sizeof block);
    writeWorld(fw, w, d.source);
    const RomShared shared(fw);
    Vc2OscBlockHead head {};
    Vc2OscAnalogWords words {};
    std::memcpy(&head, block, sizeof head);
    std::memcpy(&words, block + kVc2OscAnalogWordsAt, sizeof words);
    const auto port = [&] {
        vc2OscBlockConvert(in, row, w, shared, head);
        if (!vocoder) vc2OscAnalogConvert(in, w, shared, words);
    };
    std::string what = vc2BothOn(fw, vocoder ? kVocoder : kAnalog, { kInputAt, kBlockAt }, "the converter", port,
                                 [] { return std::string(); });
    std::memcpy(block, &head, sizeof head);
    std::memcpy(block + kVc2OscAnalogWordsAt, &words, sizeof words);
    const size_t compared = (size_t)(vocoder ? kCompared : kAnalogCompared);
    if (what.empty()) {
        what = vc2RegionsDifference(fw, { { kBlockAt, block, compared, "the block" },
                                          { kInputAt, in, kInputBytes, "the input" } });
    }
    return shared.faulted && what.empty() ? "a shared routine faults: " + fw.cpu.error() : what;
}
} // namespace

long vc2OscConvertCases(Vc2Fw& fw, const Vc2Run& run) {
    vc2HookGroups(fw, kOscSumsAt, kLfoSumsAt);
    Draws d { std::mt19937((uint32_t)run.seed + 2), std::mt19937((uint32_t)run.seed + 3) };
    fw.cpu.hook(kPitchSource, [&d](Sh4& cpu) {
        cpu.fb[0][0] = std::bit_cast<uint32_t>(d.source);
        cpu.pc = cpu.pr;
    });
    Counts& n = d.n;
    Vc2Tally tally { "XT_VC2CARRIER_FIRST", "XT_VC2CARRIER_STOP" };
    for (int c = 0; c < run.cases; ++c) {
        std::string what;
        for (int k = 0; k < kConversions && what.empty(); ++k, ++n.conversions) what = conversion(fw, d);
        if (vc2CaseOutcome(tally, what, c, n.conversions)) break;
    }
    std::printf("osc block: %d cases, %ld conversions (%ld the VOCODER's; Pitch KF 0 %ld, 100%% %ld; rows past 10 "
                "%ld), %ld different\n", run.cases, n.conversions, n.vocoder, n.kfZero, n.kfWhole, n.rowsPast,
                tally.different);
    std::printf("analog words: %ld conversions (%ld with the sub oscillator, %ld with a Slope, %ld LA-SQUARE's)\n",
                d.a.conversions, d.a.sub, d.a.slope, d.a.laSquare);
    return tally.different;
}
#endif
