// xt_vc2osc's records (v238; v230's, moved here from Vc2OscCheck.cpp and Vc2OscFakes.cpp as they grew): what the
// voice reads where its words point (the wave headers, the loop records, the bound envelopes and LFOs, the block's
// +0xe0 records), their addresses in the interpreter's RAM, their draws and their writing into rom.4's memory, where
// both sides read them.  XT_VC2OSC_MAIN only.
#ifdef XT_VC2OSC_MAIN
#include "Vc2OscSides.h"

namespace {
// [FW] where a record's fields are (Vc2OscVoice.h's): the wave header's, the loop record's
constexpr uint32_t kHeaderFlag = 2, kHeaderRate = 8, kHeaderKey = 0xa, kHeaderCents = 0x18, kHeader1c = 0x1c;
constexpr uint32_t kLoopStart = 4, kLoopEnd = 8, kBwdEnd = 0xc, kBwdStart = 0x10, kRateFwd = 0x14, kRateBwd = 0x16;
constexpr uint32_t kFwdMode = 0x18, kBwdMode = 0x19, kLoopFine = 0x1b;
constexpr uint32_t kLoopsFrom = 0x100, kEnvelopesFrom = 0x200, kLfosFrom = 0x400, kBoundStride = 0x80;
constexpr uint32_t kPairStride = 0x400;
// [FW] a bound envelope's words the targets read (+0x1c, at 0 one time in two, +0x2c, +0x4c) and an LFO's (+0x38)
constexpr std::array<uint32_t, 3> kEnvelopeWords { 0x1c, 0x2c, 0x4c };
constexpr uint32_t kEnvelopeOut = 0x4c, kLfoOut = 0x38;

// A loop record's tune: within ±12 semitones most often, any word one time in four, and one time in eight about where
// the factor, 2^(−tune/120000), leaves the floats' range (2^∓126…2^∓151), so the note-on's test of it meets a 0
int32_t drawTune(std::mt19937& rng) {
    constexpr std::array<int32_t, 6> kOctaves { 126, 127, 128, 149, 150, 151 };
    constexpr int32_t kPerOctave = 120000;   // [FW] 0x893ae644's 1/120000
    if (rng() % 8 == 0) {
        const int32_t sign = rng() % 2 ? 1 : -1;
        return sign * kOctaves[rng() % kOctaves.size()] * kPerOctave + (int32_t)(rng() % 3) - 1;
    }
    return rng() % 4 == 0 ? (int32_t)rng() : (int32_t)(rng() % 2400) - 1200;
}

// A loop record (v238: its loops, rates and modes as any words; the rates' exp2 stays within ±2.7 octaves)
Vc2OscLoopRecord drawLoop(std::mt19937& rng) {
    Vc2OscLoopRecord loop;
    loop.tune = drawTune(rng);
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
} // namespace

uint32_t oscHeaderAt(int k) { return kOscRecordsAt + kOscRecordStride * (uint32_t)k; }
uint32_t oscLoopAt(int k) { return kOscRecordsAt + kLoopsFrom + kOscRecordStride * (uint32_t)k; }
uint32_t oscEnvelopeAt(int k) { return kOscRecordsAt + kEnvelopesFrom + kBoundStride * (uint32_t)k; }
uint32_t oscLfoAt(int k) { return kOscRecordsAt + kLfosFrom + kBoundStride * (uint32_t)k; }
uint32_t oscPairAt(int k) { return kOscPairsAt + kPairStride * (uint32_t)k; }

void oscDrawRecords(OscRecords& r, std::mt19937& rng) {
    constexpr std::array<uint16_t, 5> kRates { 44100, 32000, 22050, 48000, 0 };
    for (int k = 0; k < kOscRecords; ++k) {
        Vc2OscWaveHeader& h = r.headers[(size_t)k];
        h.flag = rng() % 3 == 0 ? (uint8_t)rng() : (uint8_t)(rng() % 2);
        h.rate = rng() % 3 == 0 ? (uint16_t)rng() : kRates[rng() % kRates.size()];
        h.rootKey = (int8_t)rng();
        h.cents = rng() % 4 == 0 ? (int32_t)rng() : (int32_t)(rng() % 128);
        h.word1c = (uint32_t)rng();
        r.loops[(size_t)k] = drawLoop(rng);
        for (uint32_t word : kEnvelopeWords) r.bound[oscEnvelopeAt(k) + word] = oscDrawFloat(rng);
        if (rng() % 2) r.bound[oscEnvelopeAt(k) + kEnvelopeWords[0]] = 0.0f;
        // the outputs the pitch and the formant take, on their way to exp2
        r.bound[oscEnvelopeAt(k) + kEnvelopeOut] = oscDrawFloat(rng, OscRange::moderate);
        r.bound[oscLfoAt(k) + kLfoOut] = oscDrawFloat(rng, OscRange::moderate);
        // the backward loop's rate's factor (v238): no NaN, which a product with another NaN would leave to the host
        r.bound[oscPairAt(k) + kOscPairWord] = oscDrawFloat(rng, OscRange::noNaN);
    }
}

void oscWriteRecords(Vc2Fw& fw, const OscRecords& r) {
    for (int k = 0; k < kOscRecords; ++k) {
        const Vc2OscWaveHeader& h = r.headers[(size_t)k];
        put(fw, oscHeaderAt(k) + kHeaderFlag, h.flag);
        put(fw, oscHeaderAt(k) + kHeaderRate, h.rate);
        put(fw, oscHeaderAt(k) + kHeaderKey, h.rootKey);
        put(fw, oscHeaderAt(k) + kHeaderCents, h.cents);
        put(fw, oscHeaderAt(k) + kHeader1c, h.word1c);
        putLoop(fw, oscLoopAt(k), r.loops[(size_t)k]);
    }
    for (const auto& [at, x] : r.bound) put(fw, at, x);
}
#endif
