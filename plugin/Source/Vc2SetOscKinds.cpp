// An OSC block's kind: see Vc2SetOscKinds.h (v391).  The floats through XtFloat, as the SH-4 rounds; the analog's and
// the VOCODER's converters the ported ones, on the block's words copied in and out.
#include "Vc2SetOscKinds.h"
#include "Vc2OscAnalogConvert.h"
#include "Vc2OscExtinConvert.h"
#include "Vc2OscPcmConvert.h"
#include "Vc2SetChildHead.h"
#include "Vc2SetEnvelopeTables.h"
#include "Vc2SetOscBlock.h"
#include "XtFloat.h"
#include <cstring>

using xtf::add;
using xtf::flt;
using xtf::mul;

namespace {
constexpr uint32_t kPcmStart = 0x8939e120;   // [FW] not ported
// [FW] what PCM's converter calls by the wave's type (+0xa7, 0…5): LITE's words 0x8939efa0 and VariPhrase's
// 0x8939f2a0, not ported; 0 none
constexpr uint32_t kWaveTypeAt = 0xa7;
constexpr std::array<uint32_t, 6> kWaveWords { 0x8939efa0, 0x8939f2a0, 0, 0x8939f2a0, 0, 0x8939f2a0 };
constexpr uint32_t kExtinSystemAt = 3, kExtinHeaderAt = 0x1e;          // [FW] EXTIN's converter's two bytes
constexpr uint32_t kAnalogKind = 3, kExtinKind = 4, kVocoderKind = 5;   // [FW] the starts' kinds
constexpr uint32_t kSwitchAt = 0, kTypeAt = 1;           // [FW] the record's OSC switch and type
constexpr uint32_t kVibratoTypeAt = 0x1c, kRateAt = 0x1d, kDepthAt = 0x1e;   // [FW] the record's vibrato bytes
// [FW] the block's words: the tempo, a flag, two zeros and four ones (the starts'); the vibrato's; the kind's own
constexpr size_t kTempoAt = 0x60 / 4, kFlagAt = 0x64 / 4, kZerosAt = 0x68 / 4, kOnesAt = 0x70 / 4, kOnes = 4;
constexpr size_t kVibratoAt = 0x80 / 4, kKindWordsAt = 0xb0 / 4;
// [FW] the vibrato's words from +0x80: the row's wave, the rate, five of the row's words, a fall of 0, a ratio of 1,
// the velocity ÷ 2, the depth and its level; the row's words copied and the level's scale in it
constexpr size_t kRateWord = 1, kCopiedWord = 2, kCopied = 5, kFallWord = 7, kRatioWord = 8, kScaleWord = 9;
constexpr size_t kDepthWord = 10, kLevelWord = 11, kRowCopiedAt = 6, kRowLevelAt = 19;
constexpr float kRateFrom = 2.0f;          // fldi1, added to itself
constexpr int kLfoPitch = 1;               // [FW] the LFO group's Pitch sum
constexpr int kDepthTop = 63;              // [FW] a depth's hold
constexpr uint32_t kBeamAt = 10, kWidthBeamAt = 8;   // [FW] M's +0x182 and +0x180, from its controllers at +0x178
constexpr uint32_t kSystemAt = 5, kSawSlopeAt = 0xa, kSquareSlopeAt = 0xc, kCalibrationAt = 0xe;   // [FW] the systems'

const Vc2SetEnvelopeTables& setTables() { return vc2SetEnvelopeTables(); }

uint8_t* bytesOf(Vc2SetOscInput& w) { return reinterpret_cast<uint8_t*>(&w); }

uint16_t u16(const uint8_t* b, uint32_t at) {
    uint16_t v;
    std::memcpy(&v, b + at, sizeof v);
    return v;
}

// The analog's and EXTIN's starts (and PCM's words before its kind): the kind, 0 at +0x68 and +0x6c, 1 at +0x70…
// +0x7c, the clock's tempo ÷ `perTenth` at +0x60, 0 at +0x64
void kindStart(uint32_t* block, uint32_t kind, float perTenth, Vc2SetReads& reads) {
    block[0] = kind;
    block[kZerosAt] = floatWord(0.0f);
    block[kZerosAt + 1] = floatWord(0.0f);
    for (size_t k = 0; k < kOnes; ++k) block[kOnesAt + k] = floatWord(1.0f);
    block[kTempoAt] = floatWord(xtf::div(flt(static_cast<int32_t>(vc2SetTempo(reads.clock()))), perTenth));
    block[kFlagAt] = 0;
}

// 0x8939c260: a depth with its sum, held to ±63, squared, × `scale`, held to ±1
float levelDepth(int16_t depth, int16_t sum, float scale) {
    const float d = vc2OscDepthSquared(vc2OscHeld(depth + sum, -kDepthTop, kDepthTop), vc2OscConvertTables());
    return vc2OscHeld(mul(d, scale), -1.0f, 1.0f);
}

// The vibrato's words from +0x80 by the Type's row: its wave and five of its words, the rate × 6/127 + 2 (FMAC), a
// fall of 0, a ratio of 1, the velocity ÷ 2, the depth with the LFO's Pitch sum squared × 300, and its level by the row's
// scale; the getter called once for each, as rom.4 calls it
void vibratoOf(const Vc2SetOscInput& in, uint32_t* block, Vc2SetReads& reads) {
    const auto& row = setTables().vibrato[in.record[kVibratoTypeAt]];
    uint32_t* v = block + kVibratoAt;
    v[0] = row[0];
    for (size_t k = 0; k < kCopied; ++k) v[kCopiedWord + k] = row[kRowCopiedAt + 2 * k];
    v[kFallWord] = floatWord(0.0f);
    v[kRatioWord] = floatWord(1.0f);
    v[kScaleWord] = in.velocity >> 1;
    v[kRateWord] = floatWord(add(mul(flt(in.record[kRateAt]), setTables().vibratoRate), kRateFrom));
    const int16_t depth = static_cast<int8_t>(in.record[kDepthAt]);
    const int16_t sum = reads.oscGroups(in.matrix, in.osc).lfo[kLfoPitch];
    const float squared = vc2OscDepthSquared(vc2OscHeld(depth + sum, -kDepthTop, kDepthTop), vc2OscConvertTables());
    v[kDepthWord] = floatWord(mul(squared, setTables().vibratoDepth));
    const int16_t again = reads.oscGroups(in.matrix, in.osc).lfo[kLfoPitch];
    v[kLevelWord] = floatWord(levelDepth(depth, again, std::bit_cast<float>(row[kRowLevelAt])));
}

// What the analog's and the VOCODER's converters read besides the input: the OSC's groups, M's four bytes, the pitch
// source, and the systems' analog words and calibration rows
Vc2OscConvertIn worldOf(const Vc2SetOscInput& in, Vc2SetReads& reads) {
    Vc2OscConvertIn w;
    w.sums = reads.oscGroups(in.matrix, in.osc);
    w.beam = { reads.matrixControl(in.matrix, kBeamAt), reads.matrixControl(in.matrix, kBeamAt + 1) };
    w.widthBeam = { reads.matrixControl(in.matrix, kWidthBeamAt), reads.matrixControl(in.matrix, kWidthBeamAt + 1) };
    w.pitchSource = reads.pitchSource();
    const std::array<uint8_t, 60> sys = reads.oscSystems();
    Vc2OscAnalogSystem& a = w.system;
    a.sawTone = sys[kSystemAt];
    a.squareTone = sys[kSystemAt + 1];
    a.sawToneKf = sys[kSystemAt + 2];
    a.squareToneKf = sys[kSystemAt + 3];
    a.aliasCancel = sys[kSystemAt + 4];
    a.sawSlope = u16(sys.data(), kSawSlopeAt);
    a.squareSlope = u16(sys.data(), kSquareSlopeAt);
    std::memcpy(w.calibration.data(), sys.data() + kCalibrationAt, sizeof w.calibration);
    return w;
}

// A ported converter's part on its words in the block, copied in and out
template <class Words, class Part>
void onWords(uint32_t* block, size_t at, Part part) {
    Words words;
    std::memcpy(&words, block + at, sizeof words);
    part(words);
    std::memcpy(block + at, &words, sizeof words);
}

// The analog's converter (0x8939cf70) or the VOCODER's (0x8939db00): the block's head, then the kind's own words
void portedConvert(Vc2SetOscInput& in, uint32_t* block, bool vocoder, Vc2SetReads& reads) {
    uint8_t* input = bytesOf(in);
    const Vc2OscConvertIn w = worldOf(in, reads);
    const Vc2ToneShared& shared = reads.shared();
    const int row = vocoder ? kVc2OscVocoderRow : vc2OscAnalogRow(input);
    onWords<Vc2OscBlockHead>(block, 0, [&](Vc2OscBlockHead& head) { vc2OscBlockConvert(input, row, w, shared, head); });
    if (vocoder)
        onWords<Vc2VocoderOscP>(block, kKindWordsAt,
                                [&](Vc2VocoderOscP& p) { vc2VocoderOscConvert(input, w.sums, shared, p); });
    else
        onWords<Vc2OscAnalogWords>(block, kKindWordsAt,
                                   [&](Vc2OscAnalogWords& a) { vc2OscAnalogConvert(input, w, shared, a); });
}

// Each type's start (0x8939fb40's switch) and converter (0x8939fbd0's)
void pcmStart(Vc2SetOscInput& in, uint32_t* block, Vc2SetReads& reads) {
    reads.unportedOscKind(kPcmStart, bytesOf(in), block);
}

void extinStart(Vc2SetOscInput&, uint32_t* block, Vc2SetReads& reads) {
    kindStart(block, kExtinKind, setTables().extinTenth, reads);
}

void analogStart(Vc2SetOscInput&, uint32_t* block, Vc2SetReads& reads) {
    kindStart(block, kAnalogKind, vc2OscConvertTables().perTenth, reads);
}

void vocoderStart(Vc2SetOscInput& in, uint32_t* block, Vc2SetReads& reads) {
    analogStart(in, block, reads);
    block[0] = kVocoderKind;
}

// PCM's (0x8939e300, v394): its own part, then by the wave's type LITE's words or VariPhrase's, not ported yet
void pcmConvert(Vc2SetOscInput& in, uint32_t* block, Vc2SetReads& reads) {
    vc2OscPcmConvert(bytesOf(in), worldOf(in, reads), reads.shared(), block);
    const uint8_t type = bytesOf(in)[kWaveTypeAt];
    const uint32_t words = type < kWaveWords.size() ? kWaveWords[type] : 0;
    if (words != 0) reads.unportedOscKind(words, bytesOf(in), block);
}

// EXTIN's (0x8939e9e0, v393): the systems' byte +0x10203 and the input's part's header byte +0x1e besides
void extinConvert(Vc2SetOscInput& in, uint32_t* block, Vc2SetReads& reads) {
    const Vc2OscExtinIn x { reads.oscSystems()[kExtinSystemAt], reads.oscHeader(in.part)[kExtinHeaderAt] };
    vc2OscExtinConvert(bytesOf(in), worldOf(in, reads), x, reads.shared(), block);
}

void vocoderConvert(Vc2SetOscInput& in, uint32_t* block, Vc2SetReads& reads) { portedConvert(in, block, true, reads); }

void analogConvert(Vc2SetOscInput& in, uint32_t* block, Vc2SetReads& reads) { portedConvert(in, block, false, reads); }

using KindWay = void (*)(Vc2SetOscInput&, uint32_t*, Vc2SetReads&);
struct KindWays {
    KindWay start, convert;
};
// [FW] by the OSC's type less 1, unsigned: PCM, EXTIN, the VOCODER; type 0 and past 3 the analog's
constexpr std::array<KindWays, 3> kWays { { { pcmStart, pcmConvert }, { extinStart, extinConvert },
                                            { vocoderStart, vocoderConvert } } };
constexpr KindWays kAnalogWays { analogStart, analogConvert };

const KindWays& waysOf(const Vc2SetOscInput& in) {
    const uint32_t way = in.record[kTypeAt] - 1u;
    return way < kWays.size() ? kWays[way] : kAnalogWays;
}
} // namespace

void vc2SetOscStart(Vc2SetOscInput& in, uint32_t* block, Vc2SetReads& reads) {
    if (in.record[kSwitchAt] == 0) {
        block[0] = 0;
        return;
    }
    waysOf(in).start(in, block, reads);
}

void vc2SetOscConvert(Vc2SetOscInput& in, uint32_t* block, Vc2SetReads& reads) {
    if (block[0] == 0) return;
    vibratoOf(in, block, reads);
    waysOf(in).convert(in, block, reads);
}
