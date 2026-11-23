// The band players' waves and states for xt_vc2osc and xt_vc2tone (v332): a band wave built into the wave memory (its
// parts Vc2OscBandVpdt.cpp's), the object's words as VariPhrase's note-on leaves them before a band player's set-up,
// rom.4's own set-up on them, and the states it leaves, captured before any hook, which a render through a band player
// is steered to; from v333 a band note-on's block given a captured wave's records, and a band re-seek's object a
// captured state (Vc2OscDraws.h says what each draws).  XT_VC2OSC_MAIN or XT_VC2TONE_MAIN only.
#if defined(XT_VC2OSC_MAIN) || defined(XT_VC2TONE_MAIN)
#include "Vc2OscDraws.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace {
constexpr int kBands = 6;
constexpr int32_t kSixteen = 16;
constexpr uint32_t kRateFwdAt = 0x14, kRateBwdAt = 0x16;   // [FW] a loop record's rates, tenths of a cent
// [FW] the head's records in the voice (its +0x108 on, VariPhrase's note-on's): the vpdt, the bands' data, the header,
// the loop record, the cached events; the head itself (+0x1a0, the object's +0x44)
constexpr uint32_t kHeadOf = 0x108, kHeadAt = 0x1a0, kBandsOf = 0x10c, kHeaderOf = 0x128, kLoopOf = 0x130;
constexpr uint32_t kCachedOf = 0x134;
// [FW] the object's words a note-on gives (the voice's offsets): byte 0, byte 2, byte 3, the range by events and in
// samples, the tempos, the pitch, the walk, the gain's target and the gain
constexpr uint32_t kFlagsAt = 0x15c, kEventBitsAt = 0x15e, kWalkBitsAt = 0x15f, kFirstEventAt = 0x170;
constexpr uint32_t kLastEventAt = 0x172, kStartAt = 0x174, kEndAt = 0x178, kOrigAt = 0x1a4, kTempoAt = 0x1a8;
constexpr uint32_t kPitchAt = 0x1cc, kWalkAt = 0x1d8, kTargetAt = 0x1dc, kGainAt = 0x1e0;
// [FW] byte 0: the loops on, Tempo Sync, Time Trip, the wrap, Vari off, stereo, the range in samples
constexpr uint8_t kLoopsOn = 0x01, kSync = 0x02, kTrip = 0x04, kWraps = 0x08, kVariOff = 0x20, kStereo = 0x40;
constexpr uint8_t kInSamples = 0x80;
constexpr uint32_t kLeastTempo = 400000, kTempoSpan = 2100000;   // [FW] 40…250 BPM, ×10000

uint32_t aligned4(uint32_t at) { return (at + 3) & ~3u; }

// Byte 0, Vari off never with Tempo Sync or Time Trip: under the beat Vari off reads at the walk's step alone, which
// the beat's factor takes up to 32 times, past a record a step (bandSegments).  ENSEMBLE's loops off: walking back
// below the backward loop's start with them on, the next crossing is that point, One Way's jump takes the position
// below 0, and ENSEMBLE reads its frames' index unsigned, far from the wave (BACKING runs the same loops' code)
uint8_t bandFlags(const OscBandWave& w, bool ensemble, std::mt19937& rng) {
    uint8_t f = w.stereo ? kStereo : 0;
    if (rng() % 4 != 0) f |= kInSamples;
    if (w.looped && !ensemble && rng() % 3 != 0) f |= kLoopsOn;
    if (rng() % 8 == 0) f |= kSync;
    if (rng() % 8 == 0) f |= kTrip;
    if (rng() % 4 == 0 && (f & (kSync | kTrip)) == 0) f |= kVariOff;
    if (rng() % 4 == 0) f |= kWraps;
    return f;
}

float bandWalk(std::mt19937& rng) {
    if (rng() % 8 == 0) return 0.0f;
    return (rng() % 2 ? 1.0f : -1.0f) * oscWithin(rng, 0.1f, 1.5f);
}

// The range's last event: the first or one past it, never past the wave's last (the wrap would read an entry from what
// follows the table, which rom.4's set-up doesn't hold the range to); ENSEMBLE's the wave's last, where a backward wrap
// lands at the end: from an event before its step back the position goes below 0
uint16_t bandLastEvent(const OscBandWave& w, uint32_t first, bool ensemble, std::mt19937& rng) {
    if (ensemble) return (uint16_t)(w.events - 1);
    return (uint16_t)(first + rng() % ((uint32_t)w.events - first));
}

// [FW] v333: VariPhrase's kind; the wave types whose start sets a band player up (BACKING 3, ENSEMBLE 5) and whose
// re-seek is the band players' seek (3…5); a note-on block's flags byte (the voice's +0x138), the range's two ends and
// the seek point (+0x13c, +0x140, +0x144)
constexpr uint32_t kVariPhrase = 2, kEnsembleType = 5;
constexpr std::array<uint32_t, 2> kSetUpTypes { 3, 5 };
constexpr std::array<uint32_t, 3> kSeekTypes { 3, 4, 5 };
constexpr uint32_t kBlockFlagsOf = 0x138, kFromOf = 0x13c, kToOf = 0x140, kSeekOf = 0x144;

bool bandPlayer(uint32_t type, std::span<const uint32_t> types) {
    return std::ranges::find(types, type) != types.end();
}

// The object one of the captured states of a player, from the band players' generator
void takeBandState(Vc2OscVoice& o, OscShared& shared, bool ensemble) {
    std::vector<const OscBandState*> mine;
    for (const OscBandState& st : shared.bandStates) {
        if (st.ensemble == ensemble) mine.push_back(&st);
    }
    const OscBandState& st = *mine[shared.bandRng() % mine.size()];
    std::memcpy(o.own.data() + (Vc2OscVoice::kObject - Vc2OscVoice::kOwn), st.object.data(), st.object.size());
}
} // namespace

OscBandWave oscDrawBandWave(Vc2Fw& fw, OscShared& shared, uint32_t at, uint32_t room, std::mt19937& rng) {
    OscBandWave w { at, room, rng() % 2 == 0, false, 0, 2 + (int)(rng() % 7), at, 0, 0, 0, {}, {} };
    w.frames = 600 + (int)(rng() % 4200);
    std::vector<uint8_t> vpdt = oscBandVpdt(w.frames, w.events, w.stereo, rng);
    while (oscBandRoom(w.frames, w.stereo, (uint32_t)vpdt.size()) > room) {
        w.frames = w.frames * 3 / 4;
        vpdt = oscBandVpdt(w.frames, w.events, w.stereo, rng);
    }
    w.positions = oscBandPositions(vpdt, w.stereo);
    std::vector<uint8_t> image(room, 0);
    std::memcpy(image.data(), vpdt.data(), vpdt.size());
    uint32_t next = aligned4((uint32_t)vpdt.size());
    const XtLoop loop = oscBandLoop(w.frames, rng);
    w.looped = loop.on;
    w.cached = at + next;
    oscBandCachedEvents(image.data() + next, vpdt, w.stereo, loop);
    next += kOscBandCachedBytes;
    w.loop = at + next;
    for (uint32_t rate : { kRateFwdAt, kRateBwdAt }) {
        const int16_t tenths = (int16_t)((int32_t)(rng() % 2401) - 1200);
        std::memcpy(image.data() + next + rate, &tenths, sizeof tenths);
    }
    w.header = w.loop + kOscBandRecordRoom;
    next += 2 * kOscBandRecordRoom;
    for (int k = 0; k < kBands; ++k) {
        next += kOscBandTail;
        w.bands[(size_t)k] = at + aligned4(next);
        next = aligned4(next) + oscBandFrames(k, w.frames) * (w.stereo ? 4 : 2);
    }
    for (uint32_t i = w.bands[0] - at; i + 1 < next; i += 2) {
        const int16_t x = (int16_t)((int32_t)(rng() % 16384) - 8192);
        std::memcpy(image.data() + i, &x, sizeof x);
    }
    std::memcpy(reinterpret_cast<uint8_t*>(shared.wave.data()) + (at - kOscWaveAt), image.data(), room);
    fw.cpu.write(at, image.data(), room);
    return w;
}

void oscBandNote(Vc2OscVoice& o, const OscBandWave& w, uint32_t voiceAt, bool ensemble, std::mt19937& rng) {
    const auto object = o.own.begin() + (Vc2OscVoice::kObject - Vc2OscVoice::kOwn);
    std::fill(object, object + Vc2OscVoice::kObjectBytes, 0);
    o.setU32(kHeadAt, voiceAt + kHeadOf);
    o.setU32(kHeadOf, w.vpdt);
    for (int k = 0; k < kBands; ++k) o.setU32(kBandsOf + 4 * (uint32_t)k, w.bands[(size_t)k]);
    for (const auto& [at, word] : { std::pair { kHeaderOf, w.header }, { kLoopOf, w.loop }, { kCachedOf, w.cached } }) {
        o.setU32(at, word);
    }
    o.setU8(kFlagsAt, bandFlags(w, ensemble, rng));
    o.setU8(kEventBitsAt, rng() % 4 == 0 ? (uint8_t)(rng() & 7) : 0);
    o.setU8(kWalkBitsAt, (uint8_t)rng());
    const uint32_t first = rng() % (uint32_t)w.events;
    o.setU16(kFirstEventAt, (uint16_t)first);
    o.setU16(kLastEventAt, bandLastEvent(w, first, ensemble, rng));
    const int32_t start = kSixteen * (int32_t)(rng() % (uint32_t)(w.frames / 2));
    o.setU32(kStartAt, (uint32_t)start);
    o.setU32(kEndAt, (uint32_t)(start + kSixteen * (1 + (int32_t)(rng() % (uint32_t)(w.frames - start / kSixteen)))));
    for (uint32_t at : { kOrigAt, kTempoAt }) o.setU32(at, kLeastTempo + (uint32_t)(rng() % kTempoSpan));
    o.setF32(kPitchAt, std::exp2(oscWithin(rng, -2.0f, 2.0f)));
    o.setF32(kWalkAt, bandWalk(rng));
    for (uint32_t at : { kTargetAt, kGainAt }) o.setF32(at, oscWithin(rng, 0.0f, 2.0f) / 65536.0f);
}

std::string oscBandSetUp(Vc2Fw& fw, Vc2OscVoice& o, uint32_t voiceAt, bool ensemble) {
    fw.cpu.write(voiceAt + Vc2OscVoice::kOwn, o.own.data(), o.own.size());
    if (!fw.call(kOscBandSetUps[ensemble ? 1 : 0], { voiceAt + Vc2OscVoice::kObject })) {
        return "rom.4's band player's set-up faults: " + fw.cpu.error();
    }
    fw.cpu.read(voiceAt + Vc2OscVoice::kOwn, o.own.data(), o.own.size());
    return "";
}

std::string oscCaptureBandStates(Vc2Fw& fw, OscShared& shared, uint32_t voiceAt) {
    constexpr int kNotes = 4;
    Vc2OscVoice o {};
    std::mt19937& rng = shared.bandRng;
    for (uint32_t half = 0; half < 2; ++half) {
        OscBandWave w {};
        do w = oscDrawBandWave(fw, shared, kOscBandAt + half * kOscBandBytes / 2, kOscBandBytes / 2, rng);
        while (w.stereo != (half == 1));
        shared.bandWaves.push_back(w);
        for (int n = 0; n < 2 * kNotes; ++n) {
            const bool ensemble = n >= kNotes;
            oscBandNote(o, w, voiceAt, ensemble, rng);
            const std::string what = oscBandSetUp(fw, o, voiceAt, ensemble);
            if (!what.empty()) return what;
            OscBandState st { ensemble, {} };
            std::memcpy(st.object.data(), o.at(Vc2OscVoice::kObject), st.object.size());
            shared.bandStates.push_back(st);
        }
    }
    return "";
}

long oscSteerBands(Vc2OscVoice& o, OscShared& shared) {
    if (!oscRendersBy(o, kOscBandRenders) || shared.bandStates.empty()) return 0;
    takeBandState(o, shared, o.tail.render == kOscBandRenders[1]);
    std::mt19937& rng = shared.bandRng;
    o.setF32(kPitchAt, std::exp2(oscWithin(rng, -2.0f, 2.0f)));
    o.setF32(kWalkAt, bandWalk(rng));
    ++shared.bandSteers;
    return 1;
}

void oscSteerBandBlock(std::span<uint32_t> block, OscShared& shared) {
    const auto word = [block](uint32_t at) -> uint32_t& { return block[(at - Vc2OscVoice::kOwn) / 4]; };
    const uint32_t type = word(Vc2OscVoice::kWave);
    if (word(Vc2OscVoice::kKind) != kVariPhrase || !bandPlayer(type, kSetUpTypes) || shared.bandWaves.empty()) return;
    std::mt19937& rng = shared.bandRng;
    const OscBandWave& w = shared.bandWaves[rng() % shared.bandWaves.size()];
    const bool ensemble = type == kEnsembleType;
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
        to = bandLastEvent(w, from, ensemble, rng);
    }
    word(kFromOf) = from;
    word(kToOf) = to;
    word(kSeekOf) = rng() % 2 ? from : from + rng() % (to - from + 1);
    ++shared.bandSetUps;
}

void oscSteerBandVoice(Vc2OscVoice& o, OscShared& shared) {
    std::array<uint32_t, kOscBlockWords> block {};
    std::memcpy(block.data(), o.own.data(), sizeof block);
    oscSteerBandBlock(block, shared);
    std::memcpy(o.own.data(), block.data(), sizeof block);
}

long oscSteerBandSeek(Vc2OscVoice& o, OscShared& shared) {
    const uint32_t type = o.u32(Vc2OscVoice::kWave);
    if (o.kind() != kVariPhrase || !bandPlayer(type, kSeekTypes) || shared.bandStates.empty()) return 0;
    takeBandState(o, shared, type == kEnsembleType);
    ++shared.bandSeeks;
    return 1;
}
#endif
