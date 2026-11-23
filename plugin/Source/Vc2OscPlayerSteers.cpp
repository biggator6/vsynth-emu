// The players' steers for xt_vc2osc and xt_vc2tone (v332, v333; moved from Vc2OscBandDraws.cpp in v334 and made the
// players'): a render through a band player or SOLO steered to a state rom.4's own set-up left (captured before any
// hook), a note-on's block of a player's type given one of its captured waves' records, a re-seek through a player's
// seek given a captured state.  The band players' draws come from their generator, SOLO's from its own and (v335) the
// period-jump player's from its own, so each family's stay as they were (Vc2OscDraws.h says what each draws).
// XT_VC2OSC_MAIN or XT_VC2TONE_MAIN only.
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
// [FW] VariPhrase's kind; ENSEMBLE's type; the types the band players' seek has (3…5), the record players' (1, 6, 8,
// kOscRecordTypes, with the block's 0x20 clear: else Vari off's); a note-on block's flags byte (the voice's +0x138), the
// range's two ends and the seek point (+0x13c, +0x140, +0x144)
constexpr uint32_t kVariPhrase = 2, kEnsembleType = 5;
constexpr std::array<uint32_t, 3> kBandSeekTypes { 3, 4, 5 };
constexpr uint32_t kBlockFlagsOf = 0x138, kFromOf = 0x13c, kToOf = 0x140, kSeekOf = 0x144;
constexpr uint8_t kSeekBit = 0x20;

bool among(uint32_t type, std::span<const uint32_t> types) { return std::ranges::find(types, type) != types.end(); }

// A family's draws: the band players', SOLO's or (v335) the period-jump player's generator, waves and counts
struct OscPlayerFamily {
    std::mt19937& rng;
    std::vector<OscBandWave>& waves;
    long &steers, &setUps, &seeks;
};
OscPlayerFamily familyOf(OscShared& s, OscFamily f) {
    if (f == OscFamily::solo) return { s.soloRng, s.soloWaves, s.soloSteers, s.soloSetUps, s.soloSeeks };
    if (f == OscFamily::jump) return { s.jumpRng, s.jumpWaves, s.jumpSteers, s.jumpSetUps, s.jumpSeeks };
    return { s.bandRng, s.bandWaves, s.bandSteers, s.bandSetUps, s.bandSeeks };
}
// v335: the row whose render `render` is, or none
const OscPlayerRow* rowRendering(uint32_t render) {
    const auto* row = std::ranges::find(kOscPlayers, render, &OscPlayerRow::render);
    return row == kOscPlayers.end() ? nullptr : row;
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
    const OscPlayerRow* row = rowRendering(o.tail.render);   // v335: the family by the render's row
    if (row == nullptr || shared.bandStates.empty()) return 0;
    const OscPlayerFamily f = familyOf(shared, row->family);
    if (row->family == OscFamily::band) takeBandState(o, shared, o.tail.render == kOscBandRenders[1], f.rng);
    else takeState(o, shared, oscTypesRendering(o.tail.render), f.rng);
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
    const OscPlayerFamily f = familyOf(shared, row->family);
    if (f.waves.empty()) return;
    std::mt19937& rng = f.rng;
    const OscBandWave& w = f.waves[rng() % f.waves.size()];
    word(kHeadOf) = w.vpdt;
    for (int k = 0; k < kBands; ++k) word(kBandsOf + 4 * (uint32_t)k) = w.bands[(size_t)k];
    word(kCachedOf) = w.cached;
    uint32_t& flags = word(kBlockFlagsOf);   // its low byte
    flags = (flags & ~(uint32_t)kStereo) | (w.stereo ? kStereo : 0u);
    // v335: the period-jump player's Tempo Sync and Time Trip off, as the players' part plays it (oscBandNote)
    constexpr uint32_t kJumpType = 8, kJumpOff = 0x06;   // [FW] its type; byte 0's Tempo Sync, Time Trip
    if (type == kJumpType) flags &= ~kJumpOff;
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
    if (word(Vc2OscVoice::kKind) != kVariPhrase || !among(word(Vc2OscVoice::kWave), kOscRecordTypes)) return 0;
    std::mt19937& rng = familyOf(shared, oscPlayerOf(word(Vc2OscVoice::kWave))->family).rng;   // v335: by family
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
    // [FW] v335: the period-jump player's render glides the voice's +0xdc (the gain's target), +0xe0 (the gain) and
    // +0xe4 (its rate), the block's +0x88…: gain-like, the rate 0…1, as a rate past 2 takes the glide to ∞
    constexpr uint32_t kJumpType = 8, kGainTarget = 0xdc, kGain = 0xe0, kGainRate = 0xe4;
    if (word(Vc2OscVoice::kWave) == kJumpType) {
        for (uint32_t at : { kGainTarget, kGain }) {
            word(at) = std::bit_cast<uint32_t>(oscWithin(rng, 0.0f, 2.0f) / 65536.0f);
        }
        word(kGainRate) = std::bit_cast<uint32_t>(oscWithin(rng, 0.0f, 1.0f));
    }
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
    const bool records = among(type, kOscRecordTypes) && (o.u8(kBlockFlagsOf) & kSeekBit) == 0;
    if (o.kind() != kVariPhrase || !(band || records) || shared.bandStates.empty()) return 0;
    const OscPlayerFamily f = familyOf(shared, band ? OscFamily::band : oscPlayerOf(type)->family);
    if (band) takeBandState(o, shared, type == kEnsembleType, f.rng);
    if (records) takeState(o, shared, oscTypesRendering(oscPlayerOf(type)->render), f.rng);
    ++f.seeks;
    return 1;
}
#endif
