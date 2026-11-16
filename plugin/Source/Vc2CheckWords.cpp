// The VC-2 ports' words against rom.4's: see Vc2CheckWords.h (v219).  Every address is rom.4's; the checks' builds only.
#include "Vc2CheckWords.h"
#include <vector>

namespace {
constexpr uint32_t kUnvoicedWords = 0x899d9978, kAllpassA = 0x8a0f882c, kAllpassB = 0x8a15a2ac;            // [FW]
constexpr uint32_t kAllpassAt = 0x8993fe58, kUnvoicedLevel = 0x8993fe5c;                                  // [FW]
constexpr uint32_t kLegacyWords = 0x8a1bd9d4, kLegacyDump = 0x89940254, kLegacyLevel = 0x89940258;        // [FW]
constexpr uint32_t kHiresoWords = 0x8a1bbd2c, kHalfAt = 0x899d9a88, kQuarterAt = 0x899d9ac8;              // [FW]
constexpr uint32_t kToHalfStates = 0x899d9aec, kToQuarterStates = 0x899d9b30;   // [FW] after each one's table pointer
constexpr uint32_t kDecimatorsEnd = 0x899d9b70;                                  // [FW] the second one's end
constexpr uint32_t kRatesAt = 0x2ba4, kRateBytes = 68, kHiresoLevel = 0x893f1a00;   // [FW] in the object; slot 7

// The first of `spans` that differs; empty when none does
struct Span {
    uint32_t at;
    const void* mine;
    size_t bytes;
    const char* what;
};
std::string firstOf(Vc2Fw& fw, std::initializer_list<Span> spans) {
    for (const Span& span : spans) {
        std::string what = vc2FirstDifference(fw, span.at, span.mine, span.bytes, span.what);
        if (!what.empty()) return what;
    }
    return "";
}
} // namespace

std::string vc2UnvoicedDifference(Vc2Fw& fw, const Vc2Unvoiced& port) {
    return firstOf(fw, {
        { kUnvoicedWords, &port.s, sizeof port.s, "the region" },
        { kVc2FlagsAt, port.flags.data(), sizeof port.flags, "the flags' ring" },
        { kUnvoicedLevel, &port.level, sizeof port.level, "the level" },
        { kAllpassAt, &port.allpassAt, sizeof port.allpassAt, "the allpasses' index" },
    });
}

std::string vc2UnvoicedBuffersDifference(Vc2Fw& fw, const Vc2Unvoiced& port) {
    std::string what = vc2BufferDifference(fw, kVc2MicRingAt, port.mic, "the mic ring");
    if (what.empty()) what = vc2BufferDifference(fw, kAllpassA, port.allpassA, "allpass A");
    if (what.empty()) what = vc2BufferDifference(fw, kAllpassB, port.allpassB, "allpass B");
    return what;
}

std::string vc2LegacyStateDifference(Vc2Fw& fw, const Vc2LegacyAnalysis& port) {
    return firstOf(fw, {
        { kLegacyWords, &port.s, sizeof port.s, "the state" },
        { kLegacyDump, &port.dump, sizeof port.dump, "the dump time" },
        { kLegacyLevel, &port.level, sizeof port.level, "the level" },
    });
}

std::string vc2LegacyDifference(Vc2Fw& fw, uint32_t at, const Vc2Legacy& engine) {
    std::string what = vc2FirstDifference(fw, at + 4, &engine.s, sizeof engine.s, "the object");
    return what.empty() ? vc2LegacyStateDifference(fw, engine.analysis) : what;
}

std::string vc2HiresoAnalysisDifference(Vc2Fw& fw, const Vc2HiresoAnalysis& a) {
    return firstOf(fw, {
        { kHiresoWords, &a.s, sizeof a.s, "the state" },
        { kHalfAt, a.half, sizeof a.half, "the mic at fs/2" },
        { kQuarterAt, a.quarter, sizeof a.quarter, "the mic at fs/4" },
        { kToHalfStates, a.toHalf.s, sizeof a.toHalf.s, "the first decimator" },
        { kToQuarterStates, a.toQuarter.s, sizeof a.toQuarter.s, "the second decimator" },
    });
}

void vc2HiresoAnalysisZero(Vc2Fw& fw) {
    const std::vector<uint8_t> zeros(sizeof(Vc2HiresoAnalysisWords));
    fw.cpu.write(kHiresoWords, zeros.data(), zeros.size());
    fw.cpu.write(kHalfAt, zeros.data(), kDecimatorsEnd - kHalfAt);
}

std::string vc2HiresoDifference(Vc2Fw& fw, uint32_t at, const Vc2Hireso& e) {
    std::string what = vc2FirstDifference(fw, at + 4, &e.s, sizeof e.s, "the object");
    for (int k = 0; k < Vc2Hireso::kRates && what.empty(); ++k) {
        const uint32_t rate = at + kRatesAt + kRateBytes * (uint32_t)k;
        const Vc2RateFilter& f = e.rates[k];
        what = vc2FirstDifference(fw, rate + 4, f.s, sizeof f.s, "a rate filter's states");
        if (what.empty()) {
            const size_t bytes = sizeof(float) * Vc2HiresoTables::kDecimatorWords;
            what = vc2FirstDifference(fw, fw.cpu.rd32(rate), f.c, bytes, "the words a rate filter's table points at");
        }
    }
    if (what.empty())
        what = vc2FirstDifference(fw, at + kVc2HiresoSettingWordsAt, &e.t, sizeof e.t, "the settings' words");
    if (what.empty()) what = vc2HiresoAnalysisDifference(fw, e.analysis);
    if (what.empty() && !(fw.call(kHiresoLevel, { at }) && vc2SameAsFr0(fw, e.holdLevel()))) what = "slot 7's level";
    return what;
}
