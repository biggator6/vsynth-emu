// xt_vc2osc's records (v238; v230's, moved here from Vc2OscCheck.cpp and Vc2OscFakes.cpp as they grew): what the
// voice reads where its words point (the wave headers, the loop records, the bound envelopes and LFOs, the block's
// +0xe0 records), their addresses in the interpreter's RAM, their draws and their writing into rom.4's memory, where
// both sides read them.  From v319 xt_vc2tone's too, its tones' voices reading the same.
// XT_VC2OSC_MAIN or XT_VC2TONE_MAIN only.
#if defined(XT_VC2OSC_MAIN) || defined(XT_VC2TONE_MAIN)
#include "Vc2OscDraws.h"
#include "XtWord.h"
#include <cstddef>

namespace {
// [FW] where a record's fields are (Vc2OscVoice.h's): the wave header's, the loop record's
constexpr uint32_t kHeaderFlag = 2, kHeaderRate = 8, kHeaderKey = 0xa, kHeaderCents = 0x18, kHeader1c = 0x1c;
constexpr uint32_t kHeaderFormat = 1;   // [FW] v328: the format byte mode 1's set-up reads
constexpr uint32_t kLoopStart = 4, kLoopEnd = 8, kBwdEnd = 0xc, kBwdStart = 0x10, kRateFwd = 0x14, kRateBwd = 0x16;
constexpr uint32_t kFwdMode = 0x18, kBwdMode = 0x19, kLoopFine = 0x1b;
constexpr uint32_t kLoopsFrom = 0x100, kEnvelopesFrom = 0x200, kLfosFrom = 0x400, kBoundStride = 0x80;
constexpr uint32_t kPairStride = 0x400;
// [FW] a bound envelope's words the targets read (+0x1c, at 0 one time in two, +0x2c, +0x4c) and an LFO's (+0x38)
constexpr std::array<uint32_t, 3> kEnvelopeWords { 0x1c, 0x2c, 0x4c };
constexpr uint32_t kEnvelopeOut = 0x4c, kLfoOut = 0x38;

constexpr int32_t kPerOctave = 120000;   // [FW] 0x893ae644's 1/120000: a tune's octaves

// A loop record's tune: within ±12 semitones most often, any word one time in four, and one time in eight far out,
// where rom.4's exp2 reads its 2^−n past the table's seven words (HARDWARE_FIRMWARE.md, "The VC-2's LITE note-on"):
// half of those (v240) in the middle of an octave n whose word there is 0 or a NaN (OscRecords::zeroTunes), where the
// factor, 2^(−tune/120000), meets the note-on's test; the others 126…151 octaves either way (v232's draw, made on a
// model of exp2 v240 withdrew: none of those octaves gives a 0)
int32_t drawTune(std::mt19937& rng, std::span<const int32_t> zeros) {
    constexpr std::array<int32_t, 6> kOctaves { 126, 127, 128, 149, 150, 151 };
    if (rng() % 8 == 0) {
        if (!zeros.empty() && rng() % 2) return zeros[rng() % zeros.size()] * kPerOctave + kPerOctave / 2;
        const int32_t sign = rng() % 2 ? 1 : -1;
        return sign * kOctaves[rng() % kOctaves.size()] * kPerOctave + (int32_t)(rng() % 3) - 1;
    }
    return rng() % 4 == 0 ? (int32_t)rng() : (int32_t)(rng() % 2400) - 1200;
}

// A loop record (v238: its loops, rates and modes as any words; the rates' exp2 stays within ±2.7 octaves)
Vc2OscLoopRecord drawLoop(std::mt19937& rng, std::span<const int32_t> zeros) {
    Vc2OscLoopRecord loop;
    loop.tune = drawTune(rng, zeros);
    loop.loopStart = (int32_t)rng();
    loop.loopEnd = (int32_t)rng();
    loop.bwdEnd = (int32_t)rng();
    loop.bwdStart = (int32_t)rng();
    loop.rateFwd = (int16_t)rng();
    loop.rateBwd = (int16_t)rng();
    loop.fwdMode = (int8_t)rng();
    loop.bwdMode = (int8_t)rng();
    loop.fine = (int8_t)rng();
    return loop;
}

template <class T>
void put(Vc2Fw& fw, uint32_t at, T v) {
    fw.cpu.write(at, &v, sizeof v);
}

// A loop record at `at`, field by field at its offsets
void putLoop(Vc2Fw& fw, uint32_t at, const Vc2OscLoopRecord& loop) {
    const std::array<std::pair<uint32_t, int32_t>, 5> words { { { 0, loop.tune }, { kLoopStart, loop.loopStart },
                                                               { kLoopEnd, loop.loopEnd }, { kBwdEnd, loop.bwdEnd },
                                                               { kBwdStart, loop.bwdStart } } };
    for (const auto& [offset, word] : words) put(fw, at + offset, word);
    put(fw, at + kRateFwd, loop.rateFwd);
    put(fw, at + kRateBwd, loop.rateBwd);
    put(fw, at + kFwdMode, loop.fwdMode);
    put(fw, at + kBwdMode, loop.bwdMode);
    put(fw, at + kLoopFine, loop.fine);
}

// v333: a loop record's fields by their offsets, each where Vc2OscLoopRecord holds it
using Loop = Vc2OscLoopRecord;
constexpr std::array<XtWord, 10> kLoopFields { {
    { 0, offsetof(Loop, tune), sizeof(Loop::tune) },
    { kLoopStart, offsetof(Loop, loopStart), sizeof(Loop::loopStart) },
    { kLoopEnd, offsetof(Loop, loopEnd), sizeof(Loop::loopEnd) },
    { kBwdEnd, offsetof(Loop, bwdEnd), sizeof(Loop::bwdEnd) },
    { kBwdStart, offsetof(Loop, bwdStart), sizeof(Loop::bwdStart) },
    { kRateFwd, offsetof(Loop, rateFwd), sizeof(Loop::rateFwd) },
    { kRateBwd, offsetof(Loop, rateBwd), sizeof(Loop::rateBwd) },
    { kFwdMode, offsetof(Loop, fwdMode), sizeof(Loop::fwdMode) },
    { kBwdMode, offsetof(Loop, bwdMode), sizeof(Loop::bwdMode) },
    { kLoopFine, offsetof(Loop, fine), sizeof(Loop::fine) },
} };
} // namespace

Vc2OscLoopRecord oscLoopFrom(const uint8_t* bytes) {
    Vc2OscLoopRecord loop;
    auto* to = reinterpret_cast<uint8_t*>(&loop);
    for (const auto& [at, member, n] : kLoopFields) std::memcpy(to + member, bytes + at, n);
    return loop;
}

uint32_t oscHeaderAt(int k) { return kOscRecordsAt + kOscRecordStride * (uint32_t)k; }
uint32_t oscLoopAt(int k) { return kOscRecordsAt + kLoopsFrom + kOscRecordStride * (uint32_t)k; }
uint32_t oscEnvelopeAt(int k) { return kOscRecordsAt + kEnvelopesFrom + kBoundStride * (uint32_t)k; }
uint32_t oscLfoAt(int k) { return kOscRecordsAt + kLfosFrom + kBoundStride * (uint32_t)k; }
uint32_t oscPairAt(int k) { return kOscPairsAt + kPairStride * (uint32_t)k; }

void oscFindZeroTunes(Vc2Fw& fw, OscRecords& r) {
    constexpr uint32_t kTablePool = 0x893a83e4;   // [FW] exp2's path below 0: its 2^−n table's address
    constexpr int32_t kPast = 7, kLast = 17894;   // [FW] past the table's seven words; to where n × 120000 fits a word
    const uint32_t table = fw.cpu.rd32(kTablePool);
    for (int32_t n = kPast; n <= kLast; ++n) {
        const uint32_t w = fw.cpu.rd32(table + 4 * (uint32_t)n);
        const float f = std::bit_cast<float>(w);
        if (f != f || f == 0.0f) r.zeroTunes.push_back(n);
    }
}

// The headers' rates: the converter's, then 0
constexpr std::array<uint16_t, 5> kRates { 44100, 32000, 22050, 48000, 0 };

void oscRealWave(OscRecords& r, int k, std::mt19937& rng) {
    r.headers[(size_t)k].rate = kRates[rng() % (kRates.size() - 1)];
    r.headers[(size_t)k].cents = (int32_t)(rng() % 128);
    r.loops[(size_t)k].tune = (int32_t)(rng() % 2400) - 1200;
}

void oscDrawRecords(OscRecords& r, std::mt19937& rng) {
    for (int k = 0; k < kOscRecords; ++k) {
        Vc2OscWaveHeader& h = r.headers[(size_t)k];
        h.flag = rng() % 3 == 0 ? (uint8_t)rng() : (uint8_t)(rng() % 2);
        h.rate = rng() % 3 == 0 ? (uint16_t)rng() : kRates[rng() % kRates.size()];
        h.rootKey = (int8_t)rng();
        h.cents = rng() % 4 == 0 ? (int32_t)rng() : (int32_t)(rng() % 128);
        h.word1c = (uint32_t)rng();
        r.loops[(size_t)k] = drawLoop(rng, r.zeroTunes);
        for (uint32_t word : kEnvelopeWords) r.bound[oscEnvelopeAt(k) + word] = oscDrawFloat(rng);
        if (rng() % 2) r.bound[oscEnvelopeAt(k) + kEnvelopeWords[0]] = 0.0f;
        // the outputs the pitch and the formant take, on their way to exp2
        r.bound[oscEnvelopeAt(k) + kEnvelopeOut] = oscDrawFloat(rng, OscRange::moderate);
        r.bound[oscLfoAt(k) + kLfoOut] = oscDrawFloat(rng, OscRange::moderate);
        // the backward loop's rate's factor (v238): no NaN, which a product with another NaN would leave to the host
        r.bound[oscPairAt(k) + kOscPairWord] = oscDrawFloat(rng, OscRange::noNaN);
    }
}

void oscDrawLoopEvents(OscRecords& r, std::mt19937& rng) {
    // [FW] the four events' positions, 0xb4 bytes apart: the forward loop's start and end, the backward's end and start
    constexpr std::array<uint32_t, 4> kEvents { 0x4, 0xb8, 0x16c, 0x220 };
    constexpr int32_t kFrom = 3000 * 16, kOver = 500 * 16, kSpanLeast = 64 * 16, kSpanOver = 800 * 16;
    for (int k = 0; k < kOscRecords; ++k) {
        const int32_t fwd = kFrom + (int32_t)(rng() % kOver), bwd = kFrom + (int32_t)(rng() % kOver);
        std::array<int32_t, 4> at { fwd, fwd + kSpanLeast + (int32_t)(rng() % kSpanOver),
                                    bwd + kSpanLeast + (int32_t)(rng() % kSpanOver), bwd };
        // v327: one time in four one of them at an event of the case's events' table, where a crossing meets both
        const std::vector<int32_t>& events = r.events.positions;
        if (!events.empty() && rng() % 4 == 0) {
            const size_t which = rng() % at.size();
            at[which] = events[rng() % events.size()];
        }
        for (size_t e = 0; e < at.size(); ++e) r.bound[oscPairAt(k) + kEvents[e]] = std::bit_cast<float>(at[e]);
        // v327: the word before each, an event's number and (at the forward loop's end and the backward's start) the
        // loop's mode: none, One Way, Alternate, hold, or past them
        constexpr std::array<int16_t, 6> kModes { 0, 1, 2, 3, -1, 5 };
        for (size_t e = 0; e < at.size(); ++e) {
            const uint32_t mode = e % 2 == 1 ? (uint16_t)kModes[rng() % kModes.size()] : rng() & 0xffff;
            r.bound[oscPairAt(k) + kEvents[e] - 4] = std::bit_cast<float>((rng() & 0xffff) | mode << 16);
        }
        // v328: the header's format, the wider events' (3…5) as the case's table is laid out
        constexpr std::array<uint8_t, 3> kWide { 3, 4, 5 };
        constexpr std::array<uint8_t, 5> kNarrow { 0, 1, 2, 6, 7 };
        r.headers[(size_t)k].format = r.events.wide ? kWide[rng() % kWide.size()] : kNarrow[rng() % kNarrow.size()];
    }
}

void oscWriteRecords(Vc2Fw& fw, const OscRecords& r) {
    // a header's fields, each at its place in the record (v328: a table, with the format byte)
    using H = Vc2OscWaveHeader;
    constexpr std::array<XtWord, 6> kFields { { { kHeaderFormat, offsetof(H, format), sizeof(H::format) },
                                                { kHeaderFlag, offsetof(H, flag), sizeof(H::flag) },
                                                { kHeaderRate, offsetof(H, rate), sizeof(H::rate) },
                                                { kHeaderKey, offsetof(H, rootKey), sizeof(H::rootKey) },
                                                { kHeaderCents, offsetof(H, cents), sizeof(H::cents) },
                                                { kHeader1c, offsetof(H, word1c), sizeof(H::word1c) } } };
    for (int k = 0; k < kOscRecords; ++k) {
        const auto* h = reinterpret_cast<const char*>(&r.headers[(size_t)k]);
        for (const auto& [at, member, bytes] : kFields) fw.cpu.write(oscHeaderAt(k) + at, h + member, bytes);
        putLoop(fw, oscLoopAt(k), r.loops[(size_t)k]);
    }
    for (const auto& [at, x] : r.bound) put(fw, at, x);
}
#endif
