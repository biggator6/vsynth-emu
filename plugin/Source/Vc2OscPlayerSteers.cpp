// The players' steers for xt_vc2osc and xt_vc2tone (v332, v333; moved from Vc2OscBandDraws.cpp in v334 and made the
// players'): a render through a band player or SOLO steered to a state rom.4's own set-up left (captured before any
// hook), a note-on's block of a player's type given one of its captured waves' records, a re-seek through a player's
// seek given a captured state.  The band players' draws come from their generator, SOLO's from its own, so the band
// players' stay as they were (Vc2OscDraws.h says what each draws).  XT_VC2OSC_MAIN or XT_VC2TONE_MAIN only.
#if defined(XT_VC2OSC_MAIN) || defined(XT_VC2TONE_MAIN)
#include "Vc2OscDraws.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>

namespace {
constexpr int kBands = 6;
// [FW] the head's records (the voice's offsets): the vpdt, the bands' data, the cached events; the pitch and the walk
constexpr uint32_t kHeadOf = 0x108, kBandsOf = 0x10c, kCachedOf = 0x134, kPitchAt = 0x1cc, kWalkAt = 0x1d8;
constexpr uint8_t kStereo = 0x40, kInSamples = 0x80;   // [FW] byte 0's
// [FW] VariPhrase's kind; ENSEMBLE's type; the types the band players' seek has (3…5), SOLO's (1, 6, with the block's
// 0x20 clear: else Vari off's); a note-on block's flags byte (the voice's +0x138), the range's two ends and the seek
// point (+0x13c, +0x140, +0x144)
constexpr uint32_t kVariPhrase = 2, kEnsembleType = 5;
constexpr std::array<uint32_t, 3> kBandSeekTypes { 3, 4, 5 };
constexpr std::array<uint32_t, 2> kSoloTypes { 1, 6 };
constexpr uint32_t kBlockFlagsOf = 0x138, kFromOf = 0x13c, kToOf = 0x140, kSeekOf = 0x144;
constexpr uint8_t kSeekBit = 0x20;

bool among(uint32_t type, std::span<const uint32_t> types) { return std::ranges::find(types, type) != types.end(); }

// A family's draws: the band players' or SOLO's generator, waves and counts
struct OscPlayerFamily {
    std::mt19937& rng;
    std::vector<OscBandWave>& waves;
    long &steers, &setUps, &seeks;
};
OscPlayerFamily familyOf(OscShared& s, bool solo) {
    if (solo) return { s.soloRng, s.soloWaves, s.soloSteers, s.soloSetUps, s.soloSeeks };
    return { s.bandRng, s.bandWaves, s.bandSteers, s.bandSetUps, s.bandSeeks };
}

// The object one of the captured states of the types `types`, from the family's generator
void takeState(Vc2OscVoice& o, const OscShared& shared, std::span<const uint32_t> types, std::mt19937& rng) {
    std::vector<const OscBandState*> mine;
    for (const OscBandState& st : shared.bandStates) {
        if (among(st.type, types)) mine.push_back(&st);
    }
    const OscBandState& st = *mine[rng() % mine.size()];
    std::memcpy(o.own.data() + (Vc2OscVoice::kObject - Vc2OscVoice::kOwn), st.object.data(), st.object.size());
}
// A band player's state by ENSEMBLE or not
void takeBandState(Vc2OscVoice& o, const OscShared& shared, bool ensemble, std::mt19937& rng) {
    const std::array<uint32_t, 1> type { ensemble ? kEnsembleType : 3u };
    takeState(o, shared, type, rng);
}

float steeredWalk(std::mt19937& rng) {
    if (rng() % 8 == 0) return 0.0f;
    return (rng() % 2 ? 1.0f : -1.0f) * oscWithin(rng, 0.1f, 1.5f);
}
} // namespace

long oscSteerBands(Vc2OscVoice& o, OscShared& shared) {
    const bool band = oscRendersBy(o, kOscBandRenders), solo = oscRendersBy(o, kOscSoloRenders);
    if (!(band || solo) || shared.bandStates.empty()) return 0;
    const OscPlayerFamily f = familyOf(shared, solo);
    if (band) takeBandState(o, shared, o.tail.render == kOscBandRenders[1], f.rng);
    if (solo) takeState(o, shared, kSoloTypes, f.rng);
    o.setF32(kPitchAt, std::exp2(oscWithin(f.rng, -2.0f, 2.0f)));
    o.setF32(kWalkAt, steeredWalk(f.rng));
    ++f.steers;
    return 1;
}

void oscSteerBandBlock(std::span<uint32_t> block, OscShared& shared) {
    const auto word = [block](uint32_t at) -> uint32_t& { return block[(at - Vc2OscVoice::kOwn) / 4]; };
    const uint32_t type = word(Vc2OscVoice::kWave);
    const OscPlayerRow* row = oscPlayerOf(type);
    if (word(Vc2OscVoice::kKind) != kVariPhrase || row == nullptr) return;
    const OscPlayerFamily f = familyOf(shared, row->solo);
    if (f.waves.empty()) return;
    std::mt19937& rng = f.rng;
    const OscBandWave& w = f.waves[rng() % f.waves.size()];
    word(kHeadOf) = w.vpdt;
    for (int k = 0; k < kBands; ++k) word(kBandsOf + 4 * (uint32_t)k) = w.bands[(size_t)k];
    word(kCachedOf) = w.cached;
    uint32_t& flags = word(kBlockFlagsOf);   // its low byte
    flags = (flags & ~(uint32_t)kStereo) | (w.stereo ? kStereo : 0u);
    uint32_t from = 0, to = 0;
    if ((flags & kInSamples) != 0) {
        from = rng() % (uint32_t)(w.frames / 2);
        to = from + 1 + rng() % ((uint32_t)w.frames - from);
    } else {
        from = rng() % (uint32_t)w.events;
        to = oscBandLastEvent(w, from, type == kEnsembleType, rng);
    }
    word(kFromOf) = from;
    word(kToOf) = to;
    word(kSeekOf) = rng() % 2 ? from : from + rng() % (to - from + 1);
    ++f.setUps;
}

void oscSteerBandVoice(Vc2OscVoice& o, OscShared& shared) {
    std::array<uint32_t, kOscBlockWords> block {};
    std::memcpy(block.data(), o.own.data(), sizeof block);
    oscSteerBandBlock(block, shared);
    std::memcpy(o.own.data(), block.data(), sizeof block);
}

long oscSteerSoloNote(std::span<uint32_t> block, OscShared& shared, Vc2OscVoice* o) {
    const auto word = [block](uint32_t at) -> uint32_t& { return block[(at - Vc2OscVoice::kOwn) / 4]; };
    if (word(Vc2OscVoice::kKind) != kVariPhrase || !among(word(Vc2OscVoice::kWave), kSoloTypes)) return 0;
    std::mt19937& rng = shared.soloRng;
    for (const auto& [at, range] : oscBlockFloatWords()) {
        word(at) = std::bit_cast<uint32_t>(oscDrawFloat(rng, OscRange::moderate));
    }
    // [FW] the formant's base and its envelope's and LFO's depths (the voice's offsets): within an octave, and its
    // modulation within about two, as rom.4's exp2 is exp2 only within ±7 octaves (past them it reads other tables)
    constexpr std::array<uint32_t, 2> kFormantDepths { 0x80, 0xa0 };
    constexpr uint32_t kFormantBase = 0x68;
    word(kFormantBase) = std::bit_cast<uint32_t>(oscWithin(rng, -1.0f, 1.0f));
    for (uint32_t at : kFormantDepths) word(at) = std::bit_cast<uint32_t>(oscWithin(rng, -0.001f, 0.001f));
    constexpr uint32_t kHeaderAt = 0x128, kLoopAt = 0x130;   // [FW] the block's +0xd4 and +0xdc, the voice's offsets
    oscRealWave(shared.records, 0, rng);
    word(kHeaderAt) = oscHeaderAt(0);
    word(kLoopAt) = oscLoopAt(0);
    if (o == nullptr) return 1;
    for (float* x : { &o->tail.rateRatio, &o->tail.formantFactor }) *x = oscWithin(rng, 0.5f, 2.0f);
    Vc2RampWords& formant = o->ramps[Vc2OscVoice::kFormantRamp].w;   // its value 0 would take an ∞ to a NaN
    if (!(std::isfinite(formant.value) && formant.value != 0.0f)) formant.value = oscWithin(rng, 0.5f, 2.0f);
    return 1;
}

long oscSteerSoloVoice(Vc2OscVoice& o, OscShared& shared) {
    std::array<uint32_t, kOscBlockWords> block {};
    std::memcpy(block.data(), o.own.data(), sizeof block);
    const long drawn = oscSteerSoloNote(block, shared, &o);
    std::memcpy(o.own.data(), block.data(), sizeof block);
    return drawn;
}

long oscSteerBandSeek(Vc2OscVoice& o, OscShared& shared) {
    const uint32_t type = o.u32(Vc2OscVoice::kWave);
    const bool band = among(type, kBandSeekTypes);
    const bool solo = among(type, kSoloTypes) && (o.u8(kBlockFlagsOf) & kSeekBit) == 0;
    if (o.kind() != kVariPhrase || !(band || solo) || shared.bandStates.empty()) return 0;
    const OscPlayerFamily f = familyOf(shared, solo);
    if (band) takeBandState(o, shared, type == kEnsembleType, f.rng);
    if (solo) takeState(o, shared, kSoloTypes, f.rng);
    ++f.seeks;
    return 1;
}
#endif
