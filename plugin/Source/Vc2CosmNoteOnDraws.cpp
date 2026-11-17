// xt_vc2cosm's sixth part's draws: see Vc2CosmNoteOnDraws.h (v250).
#ifdef XT_VC2COSM_MAIN
#include "Vc2CosmCheck.h"
#include "Vc2CosmNoteOnDraws.h"

namespace {
constexpr int32_t kSpeaker = 5;                     // [FW] the Speaker Simulator's code
constexpr uint32_t kBoundStride = 0x100;
constexpr uint32_t kLevel = 0x4c, kRate = 0x54, kLfoLevel = 0x38, kStartLevel = 0x58;   // [FW] the bound objects' words
// [FW] the bytes whose types have no start values case (0, 5, 10, 11, 15, 16) or the Growl's (9), and none past 20
constexpr std::array<uint8_t, 7> kPassingBytes { 0, 5, 9, 10, 11, 15, 16 };
constexpr int32_t kFirstBuffered = 7;               // [FW] the allocator's: the code's byte − 7, 0…3 and 8…11 take a slot

float moderate(std::mt19937& rng, float from, float to) { return from + (to - from) * vc2Unit(rng); }

// A code the port has a type for, or one past 20 whose byte the switches pass
int32_t drawCode(std::mt19937& rng) {
    const std::array<int32_t, 3> ported { 0, kSpeaker, Vc2CosmGrowl::kCode };
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
    if (b.code != Vc2CosmGrowl::kCode) return;
    // The Growl's converter: Intencity I/127, Speed 2·S/128 − 1, its two settings' depths and numbers
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
