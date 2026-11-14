// The splitbytes run's laws (vc2splitbytes.h): what the PCM converter makes of a split's two level bytes and its shift,
// held to a formula over every value of each byte.  SH-4 arithmetic through XtFloat, as the port's.
#include "XtFloat.h"
#include "vc2splitbytes.h"
#include <array>
#include <bit>
#include <cstdio>

namespace vc2 {

namespace {

constexpr uint32_t kLevelCurve = 0x89541f34;   // [FW] rom.4's 128 u16s, rom.2's 0x897cfcb8 (XtOsc's kLevelTab)
constexpr float kFull = 32768.0f;              // [FW] the curve at 127
constexpr uint8_t kTop = 127;
constexpr int kValues = 128, kSemitone = 1000;   // [FW] the converter's pitch: a semitone 1000 (XtOsc's xtPitch10)
constexpr float kCentsDivisor = 10.0f, kCentsLimit = 6000.0f;   // [FW] as the port's xtPitchCents
constexpr uint32_t kRootKey = 0xa8;                             // [FW] in the input
// [FW] each block word the converter's tail forms against the root key, from an input pitch: LITE's 0x8939f0c4 and
// 0x8939f10c, VariPhrase's 0x8939f894 and 0x8939f8fc
constexpr std::array kShifted{std::pair{kPitchCents, 0x94u}, std::pair{0x68u, 0x98u}};
constexpr int kSaid = 4;   // differences said
constexpr uint32_t kWaveGainId = 0x2e6;                   // [FW] OSC 1 Wave Gain, −12…+12 dB: g = 2^(dB/6)
constexpr std::array kWaveGainsDb{0, 5, -7};              // g 1, and two that round

float floatAt(const Bytes& b, uint32_t at) {
    return std::bit_cast<float>(wordAt(b, at));
}

// The level curve at a byte, /32768: exact, the divisor a power of two
float levelOf(Machine& m, uint8_t v) {
    return xtf::div((float)m.cpu.rd16(kLevelCurve + 2u * v), kFull);
}

// One case's runs against the level law, g its block +0x5c with both bytes at 127
struct LevelRuns {
    Machine& m;
    const Bytes& record;
    const SplitCase& k;
    float g = 0;
    int said = 0;

    // Block +0x5c with +0xa4 at a and +0xa6 at b against the law: 1 when they differ, said for the first few; -1 on a
    // fault
    int differs(uint8_t a, uint8_t b) {
        Bytes block;
        if (!runSplitNote(m, record, k, {{kWaveSplitByte, a}, {kMapSplitByte, b}}, block)) return -1;
        const float want = xtf::mul(xtf::mul(levelOf(m, a), g), levelOf(m, b)), got = floatAt(block, kOscGain);
        if (std::bit_cast<uint32_t>(want) == std::bit_cast<uint32_t>(got)) return 0;
        if (++said <= kSaid) std::printf("    +0xa4 %u, +0xa6 %u: block +0x5c %.9g, the law %.9g\n", a, b, got, want);
        return 1;
    }
};

// The pitch the converter's tail forms against the root key less the shift, as xtPitchCents does with the shift 0
float shiftedCents(int pitch10, int root, int shift) {
    const float c = xtf::div(xtf::flt(pitch10 - (root - shift) * kSemitone), kCentsDivisor);
    return c > kCentsLimit ? kCentsLimit : (-kCentsLimit > c ? -kCentsLimit : c);
}

} // namespace

bool checkLevelLaw(Machine& m, const SplitCase& k, int& differ) {
    differ = 0;
    for (const int db : kWaveGainsDb) {
        Bytes record, block;
        if (!setToneValue(m, kWaveGainId, db) || !recordFor(m, k.wave, record)) return false;
        if (!runSplitNote(m, record, k, {{kWaveSplitByte, kTop}, {kMapSplitByte, kTop}}, block)) return false;
        LevelRuns runs{m, record, k, floatAt(block, kOscGain)};
        int here = 0;
        for (int v = 0; v < kValues; ++v) {
            const auto b = (uint8_t)v;
            for (const auto& [x, y] : {std::pair{b, kTop}, std::pair{kTop, b}, std::pair{b, b}}) {
                const int d = runs.differs(x, y);
                if (d < 0) return false;
                here += d;
            }
        }
        std::printf("  the level law, Wave Gain %+d dB, g %.9g (both 127), over +0xa4 and +0xa6 at 0…127 alone and "
                    "together: %d of %d differ\n", db, runs.g, here, 3 * kValues);
        differ += here;
    }
    return setToneValue(m, kWaveGainId, 0);
}

bool checkShiftLaw(Machine& m, const SplitCase& k, int& differ) {
    Bytes record, block;
    if (!recordFor(m, k.wave, record)) return false;
    differ = 0;
    for (int s = 0; s < kValues * 2; ++s) {
        if (!runSplitNote(m, record, k, {{kShiftByte, (uint8_t)s}}, block)) return false;
        const int root = m.cpu.rd8(kSplitNote.input + kRootKey);
        for (const auto& [word, pitch] : kShifted) {
            const auto pitch10 = (int)m.cpu.rd32(kSplitNote.input + pitch);
            const float want = shiftedCents(pitch10, root, (int8_t)s), got = floatAt(block, word);   // mov.b: signed
            if (std::bit_cast<uint32_t>(want) == std::bit_cast<uint32_t>(got)) continue;
            if (++differ <= kSaid) std::printf("    +0xa5 %d: block +0x%02x %.9g, the law %.9g\n", s, word, got, want);
        }
    }
    std::printf("  the shift law, wave %d at key %u, block +0x08 and +0x68 against the root key less +0xa5 (−128…127): "
                "%d of %d differ\n", k.wave, k.key, differ, kValues * 2 * (int)kShifted.size());
    return true;
}

} // namespace vc2
