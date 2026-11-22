// The VC-2's OSC voice's frame: see Vc2OscVoice.h (v230).  Every address is rom.4's; each routine's switch on the kind
// is a row of Vc2OscVoiceKinds.cpp.  The settings and the getters are Vc2OscVoiceGet.cpp's.
#include "Vc2OscVoice.h"
#include "Vc2OscPlayers.h"
#include "Vc2OscTables.h"
#include "Vc2OscVariOffRender.h"
#include "Vc2OscVoiceKinds.h"
#include "XtFloat.h"
#include <algorithm>

using xtf::add;
using xtf::div;
using xtf::mul;
using xtf::sub;

namespace {
// [FW] the ramps' types as the constructor gives them (+0x90c, +0x970, +0x9d4); the start starts the pitch's (its pool
// word 0x893ae878 holds the ramp's start)
constexpr std::array<int32_t, Vc2OscVoice::kRamps> kRampTypes { 0, 2, 1 };
// [FW] the note-on's: the block's words it reads (the depth, the wave header's and the loop record's pointers) and the
// byte whose 0x10 marks a wave not pitched from its root (+0x138, the block's +0xe4)
constexpr int kDepthWord = 0x1c / 4, kHeaderWord = 0xd4 / 4, kLoopWord = 0xdc / 4;
constexpr uint32_t kDepth = 0x70, kFixedByte = 0x138, kFixed = 0x10, kRampFr4 = 0xbc, kRampFr5 = 0x5c;
// [FW] the wave types a fixed wave keeps its 440 at (0x893ae514…0x893ae520)
constexpr std::array<uint32_t, 4> kFixedTypes { 1, 6, 7, 8 };
// [FW] the object's words: its flags byte (bit 1 counts, bit 2 the matched count, bit 7 held over a seek), the count
// and the matched count's bytes, the seek's position word, its level, and the six words a take keeps
constexpr uint32_t kFlags = 0x15c, kCount = 0x19c, kMatched = 0x19d, kSeekAt = 0x1c0, kLevel = 0x1e0;
constexpr std::array<uint32_t, 6> kKept { 0x1cc, 0x1d0, 0x1d4, 0x1f0, 0x1d8, 0x1dc };
constexpr uint8_t kCounts = 2, kMatches = 4, kHeld = 0x80;
constexpr uint32_t kTakeGate = 0xb8, kSourceByte = 4, kSeekBit = 0x20, kSeekShift = 5, kRenderThird = 0x15c;

// [FW] the re-seek's players by the wave's type: kind 1's (types 0…2), kind 2's (types 1…8), each the routine with the
// block's bit 0x20 (+0x138) set and with it clear; 0 none
struct Seek {
    uint32_t set, clear;
};
constexpr std::array<Seek, 3> kSeeks1 { { { 0x893c6360, 0x893c6360 }, { 0x893cc830, 0x893cc830 },
                                          { 0x893c6de0, 0x893c6de0 } } };
constexpr std::array<Seek, 8> kSeeks2 { { { 0x893c6360, 0x893cc630 }, { 0, 0 }, { 0x893cd440, 0x893cd440 },
                                          { 0x893cd440, 0x893cd440 }, { 0x893cd440, 0x893cd440 },
                                          { 0x893c6360, 0x893cc630 }, { 0, 0 }, { 0x893c6360, 0x893cd090 } } };

float installedOf(int which, const Vc2OscTables& t) {
    const std::array<float, 3> constants { t.installed4, t.installed5, t.installed6 };
    return constants[(size_t)(which - 4)];
}

// [FW] the renders the voice has as its own (v322, v323), by the address an install writes at +0xa44: the EXT IN
// player's, kind 4's on the frame's words 0 and 1 and kind 7's on word 4 to both sides (Vc2OscNoteOn.cpp), and Vari
// off's, mono and stereo (Vc2OscVariOffRender.cpp, v261), on the wave its object's +0xf4 points into
void extInStereo(Vc2OscVoice& v, float* out, Vc2OscParts& p) { v.renderExtIn(0, 1, out, p); }
void extInMono(Vc2OscVoice& v, float* out, Vc2OscParts& p) { v.renderExtIn(4, 4, out, p); }
void variOffMono(Vc2OscVoice& v, float* out, Vc2OscParts& p) { vc2OscVariOffMono(v, out, p.waveSamples()); }
void variOffStereo(Vc2OscVoice& v, float* out, Vc2OscParts& p) { vc2OscVariOffStereo(v, out, p.waveSamples()); }
// v326: LITE's heads', mono and stereo (Vc2OscHeads.cpp), on XtLitePlayer
void headsMono(Vc2OscVoice& v, float* out, Vc2OscParts& p) { vc2OscHeads(v, out, p, false); }
void headsStereo(Vc2OscVoice& v, float* out, Vc2OscParts& p) { vc2OscHeads(v, out, p, true); }
struct OwnRender {
    uint32_t at;
    void (*render)(Vc2OscVoice&, float*, Vc2OscParts&);
};
// v327: kind 1's mode 1's (Vc2OscModeOne.cpp), one routine for mono and stereo
constexpr std::array<OwnRender, 7> kOwnRenders { { { 0x893c62e0, extInStereo }, { 0x893c61e0, extInMono },
                                                   { 0x893c63c0, variOffMono }, { 0x893c66e0, variOffStereo },
                                                   { 0x893c6e80, headsMono }, { 0x893c6e8c, headsStereo },
                                                   { 0x893c9f60, vc2OscModeOne } } };
} // namespace

bool Vc2OscVoice::renderOwn(uint32_t routine, float* out, Vc2OscParts& parts) {
    const auto* own = std::ranges::find(kOwnRenders, routine, &OwnRender::at);
    if (own == kOwnRenders.end()) return false;
    own->render(*this, out, parts);
    return true;
}

void Vc2OscVoice::construct() {
    vibrato.build();
    for (size_t ramp = 0; ramp < ramps.size(); ++ramp) ramps[ramp].construct(kRampTypes[ramp]);
    tail.sync = 0;
    tail.flag = 0;
    tail.installed = 0.0f;
    tail.seekPending = 0;
}

void Vc2OscVoice::install(Vc2OscParts& parts, const Vc2ToneShared& shared) {
    const Vc2OscKindRow& row = vc2OscKindRow(kind());
    switch (row.installs) {   // a kind's own install runs in the frame's place (v238: LITE's and VariPhrase's ported)
        case Vc2OscInstall::lite: installLite(parts, shared); return;
        case Vc2OscInstall::vari: installVari(); return;
        case Vc2OscInstall::analog: installAnalog(); return;   // v248
        case Vc2OscInstall::frame: break;
    }
    if (row.clearsFlag) tail.flag = 0;
    if (row.render != 0) tail.render = row.render;
    if (row.installed != 0) tail.installed = installedOf(row.installed, vc2OscTables());
}

void Vc2OscVoice::update(int32_t n, Vc2OscParts& parts, const Vc2ToneShared& shared) {
    if (kind() == 0) return;
    const int32_t n16 = (int16_t)n;
    vibrato.update(n16);
    const Vc2OscKindRow& row = vc2OscKindRow(kind());
    if (row.waits && tail.seekPending != 0) {
        if (f32(kLevel) > vc2OscTables().waitLevel) return;
        reseek(parts);
    }
    if (row.extInFirst) updateExtIn(parts, shared);   // v322: 0x893c6260
    for (size_t ramp = 0; ramp < ramps.size(); ++ramp) {
        if (row.ticks & (1 << ramp)) ramps[ramp].tick(n16, shared);
    }
    switch (row.updates) {   // the port's (v248, v321, v322, v324, v330), or none
        case Vc2OscUpdate::analog: updateAnalog(parts, shared); return;
        case Vc2OscUpdate::carrier: updateCarrier(parts, shared); return;
        case Vc2OscUpdate::kind6: updateKind6(parts, shared); return;
        case Vc2OscUpdate::extIn: updateExtIn(parts, shared); return;
        case Vc2OscUpdate::lite: updateLite(parts, shared); return;
        case Vc2OscUpdate::vari: updateVari(parts, shared); return;
        case Vc2OscUpdate::none: return;
    }
}

void Vc2OscVoice::reseek(Vc2OscParts& parts) {
    const uint8_t was = u8(kFlags);
    setU8(kFlags, (uint8_t)(was | kHeld));
    const int32_t at = (int32_t)u32(kSeekAt) >> kSeekShift;
    const uint32_t type = u32(kWave);
    const bool bit = (u8(kFixedByte) & kSeekBit) != 0;
    Seek seek { 0, 0 };
    if (kind() == 1 && type < kSeeks1.size()) seek = kSeeks1[type];
    if (kind() == 2 && type - 1 < kSeeks2.size()) seek = kSeeks2[type - 1];
    const uint32_t routine = bit ? seek.set : seek.clear;
    if (routine != 0) seekBy(routine, at, parts);   // v325: the voice's own seeks too
    const uint8_t now = u8(kFlags);
    setU8(kFlags, (was & kHeld) ? (uint8_t)(now | kHeld) : (uint8_t)(now & ~kHeld));
    tail.seekPending = 0;
}

void Vc2OscVoice::noteOn(const uint32_t* block, uint32_t legato, Vc2OscParts& parts, const Vc2ToneShared& shared) {
    const Vc2OscTables& t = vc2OscTables();
    std::memcpy(own.data(), block, kBlockBytes);
    setF32(kDepth, mul(std::bit_cast<float>(block[kDepthWord]), t.depthScale));
    tail.seekPending = 0;
    setU32(kLegato, legato);
    tail.flag = 0;
    const Vc2OscKindRow& row = vc2OscKindRow(kind());
    if (row.flagByWave && u32(kWave) == 1) tail.flag = 1;
    if (row.records == Vc2OscRecords::none) {
        tail.offset = 0.0f;
        tail.rateRatio = 1.0f;
        tail.detune = 0.0f;
        tail.walkFactor = 1.0f;
        tail.formantFactor = 1.0f;
        tail.hz = mul(shared.exp2(0.0f), t.plainHz);
        kindNoteOn(parts, shared);
        return;
    }
    const bool fromBlock = row.records == Vc2OscRecords::fromBlock;
    const Vc2OscWaveHeader h = parts.waveHeader(fromBlock ? block[kHeaderWord] : 0);
    const Vc2OscLoopRecord loop = parts.loopRecord(fromBlock ? block[kLoopWord] : 0);
    tail.flag = h.flag == 1 ? 1 : 0;
    tail.rateRatio = div(t.rateTop, xtf::flt(h.rate));
    const bool fixed = kind() == 2 && (u8(kFixedByte) & kFixed) != 0;
    const bool keeps = std::ranges::find(kFixedTypes, u32(kWave)) != kFixedTypes.end();
    if (keeps && fixed) {
        tail.offset = 0.0f;
        tail.detune = 0.0f;
        tail.hz = t.sampleHz;
    } else {
        tail.offset = add(mul(xtf::flt(loop.fine), t.cent), xtf::flt(h.cents));
        tail.detune = mul(xtf::flt(loop.tune), t.cent);
        const float cents = add(sub(mul(xtf::flt(h.rootKey), t.keyCents), tail.offset), tail.detune);
        const float x = shared.exp2(add(mul(cents, t.perOctave), t.exponentBase));
        tail.hz = mul(mul(x, xtf::flt(h.rate)), t.cent);
    }
    float factor = shared.exp2(mul(-xtf::flt(loop.tune), t.tuneScale));
    tail.factor = factor;
    if (!(factor > 0.0f)) {
        tail.factor = 1.0f;
        factor = 1.0f;
    }
    tail.walkFactor = factor;
    tail.formantFactor = tail.factor;
    kindNoteOn(parts, shared);
}

void Vc2OscVoice::start(const uint32_t* ramp, Vc2OscParts&, const Vc2ToneShared& shared) {
    if (vc2OscKindRow(kind()).ramp) ramps[kPitchRamp].start(ramp, f32(kRampFr4), f32(kRampFr5), shared);
}

void Vc2OscVoice::count(uint32_t x) {
    if (u8(kFlags) & kCounts) setU8(kCount, (uint8_t)(u8(kCount) + x));
}

void Vc2OscVoice::countMatched(uint32_t x) {
    if (u8(kFlags) & kMatches) setU8(kMatched, (uint8_t)x);
}

void Vc2OscVoice::legato(const uint32_t* ramp) {
    for (Vc2PitchRamp& r : ramps) r.legato(ramp);
}

uint32_t Vc2OscVoice::render(float* out, Vc2OscParts& parts) {
    switch (vc2OscKindRow(kind()).renders) {
        case Vc2OscRender::zero: std::fill(out, out + kOut, 0.0f); return 0;
        case Vc2OscRender::byRender:   // v322, v323: the voice's own renders, any other by its address
            if (!renderOwn(tail.render, out, parts)) parts.renderBy(tail.render, out, kRenderThird);
            return 0;
        case Vc2OscRender::byAnalog: return parts.renderBy(tail.renderAnalog, out, tail.sync);
        case Vc2OscRender::carrier: carrier.render(out); return 0;                                  // 0x893ebfa0
        case Vc2OscRender::kind6: kind6.render(parts.micRing(), parts.unvoiced(), out); return 0;   // 0x893ec730
    }
    return 0;
}

void Vc2OscVoice::take(const Vc2OscVoice& from) {
    if (u32(kLegato) != 0 && u32(kTakeGate) == 0) return;
    if (from.u8(kObject + kSourceByte) == 1) return;
    std::array<uint32_t, kKept.size()> kept {};
    for (size_t k = 0; k < kKept.size(); ++k) kept[k] = u32(kKept[k]);
    std::memcpy(own.data() + (kObject - kOwn), from.own.data() + (kObject - kOwn), kObjectBytes);
    for (size_t k = 0; k < kKept.size(); ++k) setU32(kKept[k], kept[k]);
    setF32(kLevel, 0.0f);
}
