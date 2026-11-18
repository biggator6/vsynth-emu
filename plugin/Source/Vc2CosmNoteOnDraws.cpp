// xt_vc2cosm's sixth part's draws: see Vc2CosmNoteOnDraws.h (v250).
#ifdef XT_VC2COSM_MAIN
#include "Vc2CosmCheck.h"
#include "Vc2CosmNoteOnDraws.h"
#include <span>

namespace {
constexpr int32_t kSpeaker = 5;                     // [FW] the Speaker Simulator's code
constexpr uint32_t kBoundStride = 0x100;
constexpr uint32_t kLevel = 0x4c, kRate = 0x54, kLfoLevel = 0x38, kStartLevel = 0x58;   // [FW] the bound objects' words
// [FW] the bytes whose start values the port has (0, 5, 10, 11, 15, 16 none; 1…4, 6, 9, 12…14, 18…20 its rows), none
// past 20
constexpr std::array<uint8_t, 18> kPassingBytes { 0, 1, 2, 3, 4, 5, 6, 9, 10, 11, 12, 13, 14, 15, 16, 18, 19, 20 };
constexpr int32_t kFirstBuffered = 7;               // [FW] the allocator's: the code's byte − 7, 0…3 and 8…11 take a slot
// [FW] the adapted types' codes (v252…v258)
constexpr int32_t kOd = 1, kDs = 2, kWs = 3, kAmp = 4, kResonator = 6, kDual = 12, kTvf = 13, kDtvf = 14, kLofi = 19;
constexpr int32_t kComp = 15, kLim = 16, kFs = 18, kTb = 20;

float moderate(std::mt19937& rng, float from, float to) { return from + (to - from) * vc2Unit(rng); }

void setBlockFloat(Vc2CosmBlockWords& b, int k, float v) { b.words[k] = std::bit_cast<uint32_t>(v); }

// A block's two settings at word `at` and five on: the first from `firstFrom` to 1, the second 0…1, their Env and LFO
// depths (a word and three on) within ±1
void drawBlockSettings(Vc2CosmBlockWords& b, std::mt19937& rng, int at, float firstFrom) {
    setBlockFloat(b, at, moderate(rng, firstFrom, 1.0f));
    for (int k : { 1, 3, 6, 8 }) setBlockFloat(b, at + k, moderate(rng, -1.0f, 1.0f));
    setBlockFloat(b, at + 5, moderate(rng, 0.0f, 1.0f));
}

// The Resonator's converter: Type 0…2, Size within ±1, Balance 0…1
void drawResBlock(Vc2CosmBlockWords& b, std::mt19937& rng) {
    b.words[0] = rng() % 3;
    drawBlockSettings(b, rng, 1, -1.0f);
}

// Lo-Fi's converter: Bit Down 0…15 and Rate Down 0…5 as the word's two low bytes, Filter 0.1…1, Balance 0…1
void drawLofiBlock(Vc2CosmBlockWords& b, std::mt19937& rng) {
    const uint32_t bit = rng() % 16, rate = rng() % 6;
    b.words[0] = (b.words[0] & 0xffff0000u) | bit | rate << 8;
    drawBlockSettings(b, rng, 1, 0.1f);
}

// OD/Dist's converter (v254): Drive and Tone 0…1 with their depths, Output Level 0…1 (word 10)
void drawOdBlock(Vc2CosmBlockWords& b, std::mt19937& rng) {
    drawBlockSettings(b, rng, 0, 0.0f);
    setBlockFloat(b, 10, moderate(rng, 0.0f, 1.0f));
}

// The Wave Shaper's: Type 0…5, Drive and Tone 0…1 with their depths, Output Level 0…1 (word 11)
void drawWsBlock(Vc2CosmBlockWords& b, std::mt19937& rng) {
    b.words[0] = rng() % 6;
    drawBlockSettings(b, rng, 1, 0.0f);
    setBlockFloat(b, 11, moderate(rng, 0.0f, 1.0f));
}

// The Amp Simulator's: Amp Type 0…2, Gain 0…1 with its depths, Bass 0…4, Middle 0…1, Treble 0…2, Output Level 0…1
void drawAmpBlock(Vc2CosmBlockWords& b, std::mt19937& rng) {
    b.words[0] = rng() % 3;
    setBlockFloat(b, 1, moderate(rng, 0.0f, 1.0f));
    for (int k : { 2, 4 }) setBlockFloat(b, k, moderate(rng, -1.0f, 1.0f));
    int k = 6;
    for (float most : { 4.0f, 1.0f, 2.0f, 1.0f }) setBlockFloat(b, k++, moderate(rng, 0.0f, most));
}

// A Q as the converters give it: Q·0.984375/127 − 1, Q 0…127
float drawnQ(std::mt19937& rng) { return (float)(rng() % 128) * 0.984375f / 127.0f - 1.0f; }

// The TVF's and the Dynamic TVF's converter: Q, Cutoff 0…1, their Env, LFO and Own depths within ±1, Filter Type 0…4
void drawTvfBlock(Vc2CosmBlockWords& b, std::mt19937& rng) {
    setBlockFloat(b, 0, drawnQ(rng));
    for (int k : { 1, 3, 5, 7, 9, 11 }) setBlockFloat(b, k, moderate(rng, -1.0f, 1.0f));
    setBlockFloat(b, 6, moderate(rng, 0.0f, 1.0f));
    b.words[12] = rng() % 5;
}

// The Dual Filter's converter (v255): Filter Type 0…2, each filter's Frequency 0…1, Env depth and LFO depth within
// ±1 and Q, from words 1 and 7
void drawDualBlock(Vc2CosmBlockWords& b, std::mt19937& rng) {
    b.words[0] = rng() % 3;
    for (int at : { 1, 7 }) {
        setBlockFloat(b, at, moderate(rng, 0.0f, 1.0f));
        setBlockFloat(b, at + 1, moderate(rng, -1.0f, 1.0f));
        setBlockFloat(b, at + 2, drawnQ(rng));
        setBlockFloat(b, at + 4, moderate(rng, -1.0f, 1.0f));
    }
}

// A word of a block and the range its converter gives it
struct Vc2CosmWordRange {
    int word;
    float from, to;
};

// The TB Filter's converter (v255): Gain 0…2, LPF Cutoff 0…1, its Keyfollow ±2, Q 0…1, Q Follow ±1, HPF Cutoff 0…1,
// its Keyfollow ±2, the Env and LFO depths within ±1; the key 0…127, a byte
constexpr Vc2CosmWordRange kTbRanges[] { { 0, 0.0f, 2.0f }, { 1, 0.0f, 1.0f }, { 2, -2.0f, 2.0f }, { 3, 0.0f, 1.0f },
                                         { 4, -1.0f, 1.0f }, { 5, 0.0f, 1.0f }, { 6, -2.0f, 2.0f }, { 7, -1.0f, 1.0f },
                                         { 9, -1.0f, 1.0f }, { 11, -1.0f, 1.0f }, { 13, -1.0f, 1.0f } };
void drawRanges(Vc2CosmBlockWords& b, std::mt19937& rng, std::span<const Vc2CosmWordRange> ranges) {
    for (const Vc2CosmWordRange& r : ranges) setBlockFloat(b, r.word, moderate(rng, r.from, r.to));
}
void drawTbBlock(Vc2CosmBlockWords& b, std::mt19937& rng) {
    drawRanges(b, rng, kTbRanges);
    b.words[15] = (b.words[15] & 0xffffff00u) | (rng() % 128);
}

// The Compressor's converter (v258): Attack and Sustain 0…1, the Level's gain 0…4
constexpr Vc2CosmWordRange kCompRanges[] { { 0, 0.0f, 1.0f }, { 1, 0.0f, 1.0f }, { 2, 0.0f, 4.0f } };
void drawCompBlock(Vc2CosmBlockWords& b, std::mt19937& rng) { drawRanges(b, rng, kCompRanges); }

// The Limiter's: the Ratio 0…3 as a byte, Threshold, Attack and Release 0…1, the Level's gain 0…4
constexpr Vc2CosmWordRange kLimRanges[] { { 1, 0.0f, 1.0f }, { 2, 0.0f, 1.0f }, { 3, 0.0f, 1.0f }, { 4, 0.0f, 4.0f } };
void drawLimBlock(Vc2CosmBlockWords& b, std::mt19937& rng) {
    b.words[0] = (b.words[0] & 0xffffff00u) | (rng() % 4);
    drawRanges(b, rng, kLimRanges);
}

// The Frequency Shifter's: Depth and Balance 0…1 with their depths, from word 0
void drawFsBlock(Vc2CosmBlockWords& b, std::mt19937& rng) { drawBlockSettings(b, rng, 0, 0.0f); }

// The Growl's converter: Intencity I/127, Speed 2·S/128 − 1, its two settings' depths and numbers
void drawGrowlBlock(Vc2CosmBlockWords& b, std::mt19937& rng) {
    Vc2CosmGrowlBlock g {};
    g.code = b.code;
    g.intensity = (float)(rng() % 128) / 127.0f;
    g.speed = moderate(rng, -1.0f, 1.0f);
    for (Vc2CosmMod* m : { &g.intensityMod, &g.speedMod }) {
        *m = {};
        if (rng() % 2 != 0) continue;
        m->env = moderate(rng, -1.0f, 1.0f);
        m->lfo = moderate(rng, -1.0f, 1.0f);
    }
    std::memcpy(&b, &g, sizeof g);
}

// Each type's block by the code's byte, its converter's way; another's noise
struct Vc2CosmBlockDraw {
    uint8_t code;
    void (*draw)(Vc2CosmBlockWords& b, std::mt19937& rng);
};
constexpr std::array<Vc2CosmBlockDraw, 14> kBlockDraws { { { kOd, drawOdBlock },
                                                           { kDs, drawOdBlock },
                                                           { kWs, drawWsBlock },
                                                           { kAmp, drawAmpBlock },
                                                           { kResonator, drawResBlock },
                                                           { (uint8_t)Vc2CosmGrowl::kCode, drawGrowlBlock },
                                                           { kDual, drawDualBlock },
                                                           { kTvf, drawTvfBlock },
                                                           { kDtvf, drawTvfBlock },
                                                           { kComp, drawCompBlock },
                                                           { kLim, drawLimBlock },
                                                           { kFs, drawFsBlock },
                                                           { kLofi, drawLofiBlock },
                                                           { kTb, drawTbBlock } } };

// A code the port has a type for, or one past 20 whose byte the switches pass
int32_t drawCode(std::mt19937& rng) {
    const std::array<int32_t, 16> ported { 0, kOd, kDs, kWs, kAmp, kSpeaker, kResonator, Vc2CosmGrowl::kCode, kDual,
                                           kTvf, kDtvf, kComp, kLim, kFs, kLofi, kTb };
    if (rng() % 4 != 0) return ported[rng() % ported.size()];
    const uint8_t low = rng() % 2 ? kPassingBytes[rng() % kPassingBytes.size()] : (uint8_t)(21 + rng() % 235);
    const uint32_t high = rng() % 2 ? 0xffffff00u : (uint32_t)(1 + rng() % 0x7fff) << 8;
    return (int32_t)(high | low);
}

bool takesSlot(int32_t code) {
    const auto k = (uint32_t)((int8_t)code - kFirstBuffered);
    return k <= 3 || (k >= 8 && k <= 11);
}
} // namespace

void vc2WriteNoteOnState(Sh4& cpu, const Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b) {
    cpu.write(kNoteOnObjectAt, &o, sizeof o);
    cpu.write(kNoteOnBlockAt, &b, sizeof b);
}

void vc2DrawNoteOnBlock(Vc2CosmBlockWords& b, std::mt19937& rng) {
    vc2CosmNoise(b, rng);
    b.code = drawCode(rng);
    vc2DrawTypeWords(b, rng);
}

void vc2DrawTypeWords(Vc2CosmBlockWords& b, std::mt19937& rng) {
    for (const Vc2CosmBlockDraw& d : kBlockDraws)
        if (d.code == (uint8_t)b.code) d.draw(b, rng);
}

uint32_t vc2DrawNoteOnPool(std::mt19937& rng) {
    const std::array<uint32_t, 4> pools { 0, 1, 2, (uint32_t)rng() };
    return rng() % 6 != 0 ? (uint32_t)(rng() % 2) : pools[rng() % pools.size()];
}

void vc2DrawNoteOnObject(Vc2CosmObjectWords& o, std::mt19937& rng) {
    vc2CosmNoise(o, rng);
    for (size_t k = 0; k < o.bindings.size(); ++k) o.bindings[k] = kNoteOnBoundAt + kBoundStride * (uint32_t)k;
    o.slot = rng() % 3 == 0 ? -1 : (int32_t)(rng() % Vc2CosmPoolWords::kSlots);
    o.pool = rng() % 4 == 0 ? (uint32_t)rng() : (uint32_t)(rng() % 2);
    o.setUp.pitch = moderate(rng, 0.25f, 4.0f);
    for (float& h : o.held) h = moderate(rng, -1.0f, 1.0f);
    Vc2CosmGrowlWords g {};
    std::memcpy(&g, &o, sizeof o);
    g.block.intensity = (float)(rng() % 128) / 127.0f;
    g.block.speed = moderate(rng, -1.0f, 1.0f);
    g.block.intensityMod = {};
    g.block.speedMod = {};
    std::memcpy(&o, &g, sizeof o);
}

void vc2DrawBound(Sh4& cpu, std::mt19937& rng) {
    for (uint32_t k = 0; k < 4; ++k) {
        const uint32_t at = kNoteOnBoundAt + kBoundStride * k;
        for (uint32_t word : { kLevel, kRate, kLfoLevel, kStartLevel })
            cpu.wr32(at + word, std::bit_cast<uint32_t>(moderate(rng, -1.0f, 1.0f)));
    }
}

int32_t vc2FillFreeSlot(Sh4& cpu, Vc2CosmPools& pools, uint32_t pool, int32_t code, std::mt19937& rng) {
    if (pool >= Vc2CosmPoolWords::kPools || !takesSlot(code)) return -1;
    for (int32_t i = 0; i < Vc2CosmPoolWords::kSlots; ++i) {
        const Vc2CosmSlot& s = pools.w.pools[pool][(size_t)i];
        if (s.inUse != 0) continue;
        for (int k = 0; k < 4; ++k) {
            std::vector<float>& mine = pools.buffer(pool, i, k);
            for (float& f : mine) f = moderate(rng, -1.0f, 1.0f);
            cpu.write(s.buffers[(size_t)k], mine.data(), mine.size() * sizeof(float));
        }
        return i;
    }
    return -1;
}
#endif
