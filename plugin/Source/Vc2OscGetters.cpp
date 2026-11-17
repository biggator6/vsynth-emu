// xt_vc2osc's getter events (v230, here from v232): each getter of the OSC voice and each of its targets on one voice
// of both sides, rom.4's answer in r0 or fr0 against the port's; and the kind's note-on (0x893af0b0) on its own, as
// the note-on ends in it.  XT_VC2OSC_MAIN only.
#ifdef XT_VC2OSC_MAIN
#include "Vc2OscSides.h"
#include "Vc2OscTargets.h"
#include <bit>
#include <cmath>

namespace {
constexpr uint32_t kKindNoteOn = 0x893af0b0;   // [FW]

// The port's answer of a getter, as its bits: a float getter alone, given the parts, given the parts and the shared
// math, a whole one, or a target
template <float (Vc2OscVoice::*Get)() const>
uint32_t alone(const Vc2OscVoice& o, Vc2OscParts&, const Vc2ToneShared&) {
    return std::bit_cast<uint32_t>((o.*Get)());
}
template <float (Vc2OscVoice::*Get)(Vc2OscParts&) const>
uint32_t asking(const Vc2OscVoice& o, Vc2OscParts& p, const Vc2ToneShared&) {
    return std::bit_cast<uint32_t>((o.*Get)(p));
}
template <float (Vc2OscVoice::*Get)(Vc2OscParts&, const Vc2ToneShared&) const>
uint32_t sharing(const Vc2OscVoice& o, Vc2OscParts& p, const Vc2ToneShared& s) {
    return std::bit_cast<uint32_t>((o.*Get)(p, s));
}
template <class Whole, Whole (Vc2OscVoice::*Get)() const>
uint32_t whole(const Vc2OscVoice& o, Vc2OscParts&, const Vc2ToneShared&) {
    return (uint32_t)(o.*Get)();
}
template <float (Vc2OscTargets::*Get)() const>
uint32_t target(const Vc2OscVoice& o, Vc2OscParts& p, const Vc2ToneShared& s) {
    return std::bit_cast<uint32_t>((Vc2OscTargets(o, p, s).*Get)());
}

// The formant getters' and the walk's and the formant's targets' own draws, half the time: kind 1 or 2 and the wave's
// type 0…2 (their paths are a kind and a type at once, too rare among the event's other draws); the position getter's,
// half the time: kind 1 three times in four, the wave's type 2 (the float position's) half the time, else 0…3, and the
// float position about its bounds
void setWave(Vc2OscVoice& o, uint32_t kind, uint32_t type) {
    constexpr uint32_t kKind = 0x54, kWave = 0x104;   // [FW]
    o.setU32(kKind, kind);
    o.setU32(kWave, type);
}
void steerSampled(Vc2OscVoice& o, std::mt19937& rng) {
    if (rng() % 2 == 0) setWave(o, 1 + rng() % 2, rng() % 3);
}
void steerPosition(Vc2OscVoice& o, std::mt19937& rng) {
    constexpr uint32_t kPositionFloat = 0x270;   // [FW]
    if (rng() % 2) return;
    setWave(o, rng() % 4 ? 1 : 2, rng() % 2 ? 2 : rng() % 4);
    o.setF32(kPositionFloat, oscDrawPosition(rng));
}

// The pitch getter's and the cents' own draw, one time in four: a NaN pitch, which their bounds let through (the
// other draws keep NaNs from what reaches exp2, where one faults)
constexpr uint32_t kPitch = 0x5c;   // [FW]
void steerNaN(Vc2OscVoice& o, std::mt19937& rng) {
    if (rng() % 4 == 0) o.setF32(kPitch, NAN);
}
// and after them, the pitch drawn again as the other draws have it, so no NaN stays for a target to take to exp2
void settlePitch(Vc2OscVoice& o, std::mt19937& rng) { o.setF32(kPitch, oscDrawFloat(rng, OscRange::moderate)); }

// [FW] the getters and the targets, each with the port's: its address, whether it answers in fr0, the port's answer's
// bits, and its own draws (none for most)
struct Getter {
    uint32_t at;
    bool inFr0;
    uint32_t (*mine)(const Vc2OscVoice&, Vc2OscParts&, const Vc2ToneShared&);
    void (*steer)(Vc2OscVoice&, std::mt19937&) = nullptr;
    void (*settle)(Vc2OscVoice&, std::mt19937&) = nullptr;   // after the event, on both sides
};
using T = Vc2OscTargets;
constexpr Getter kGetters[] = {
    { 0x893aead0, true, alone<&Vc2OscVoice::formant>, steerSampled }, { 0x893aeb00, true, alone<&Vc2OscVoice::time> },
    { 0x893aeb20, true, asking<&Vc2OscVoice::pitch>, steerNaN, settlePitch },
    { 0x893aecc0, true, sharing<&Vc2OscVoice::kindFormant>, steerSampled },
    { 0x893aecf0, true, sharing<&Vc2OscVoice::kindWalk>, steerSampled },
    { 0x893aed10, true, alone<&Vc2OscVoice::level> },
    { 0x893aed40, false, whole<int32_t, &Vc2OscVoice::word168> },
    { 0x893aed60, false, whole<uint32_t, &Vc2OscVoice::position>, steerPosition },
    { 0x893af9f0, true, target<&T::cents>, steerNaN, settlePitch }, { 0x893afb40, true, target<&T::ratio> },
    { 0x893afb80, true, target<&T::tunedRatio> }, { 0x893afd10, true, target<&T::tunedRatio> },
    { 0x893afc30, true, target<&T::walkTarget> }, { 0x893afca0, true, target<&T::formantTarget> },
    { 0x893afdf0, true, target<&T::walk>, steerSampled }, { 0x893afeb0, true, target<&T::formant>, steerSampled },
    { 0x893aff30, true, target<&T::gain> }, { 0x893aff60, true, target<&T::level> },
};
} // namespace

std::string oscOnGetter(OscSides& s, std::mt19937& rng, int v) {
    const Getter& g = kGetters[rng() % std::size(kGetters)];
    ++s.n.getters;
    if (g.steer) {
        g.steer(s.voices[(size_t)v], rng);
        oscWriteVoice(s, v);
    }
    std::string what = oscBothSides(s, g.at, { oscVoiceAt(v) }, g.inFr0 ? OscAnswer::fr0 : OscAnswer::r0,
                                    [&] { return g.mine(s.voices[(size_t)v], s.parts[(size_t)v], s.romShared); });
    if (g.settle) {
        g.settle(s.voices[(size_t)v], rng);
        oscWriteVoice(s, v);
    }
    return what;
}

void oscSteerPlayers(OscSides& s, std::mt19937& rng, int v) {
    constexpr uint32_t kKind = 0x54, kWave = 0x104, kFlagsByte = 0x138;   // [FW]
    constexpr uint8_t kVariOff = 0x20;
    Vc2OscVoice& o = s.voices[(size_t)v];
    o.setU32(kKind, 1 + rng() % 2);
    o.setU32(kWave, rng() % 10);
    if (rng() % 4) o.setU8(kFlagsByte, (uint8_t)(o.u8(kFlagsByte) | kVariOff));
    o.tail.flag = rng() % 2;   // v240: the installs' stereo rows, which only a note-on's header gave before
    oscWriteVoice(s, v);
}

namespace {
constexpr uint32_t kLiteFirst = 0x14c, kLiteFrames = 0x150;   // [FW] the block's first sample and frames (LITE's)

// v240: LITE's two heads (type 2) divide by the root's Hz (the tail's +0xa80), which an earlier note-on left: one at
// 0, past the floats or a NaN is drawn again as a real wave's, as Vc2OscEvents.cpp's steerTwoHeads gives a whole
// note-on (at 0 Hz the walk's start is 0 × ∞, whose FTRC xtf takes otherwise). One time in two a root of 44100 / 2^j
// Hz, a half period of 2^j samples exactly, with frames a multiple of it, so the walk's start from the last sample is
// one under a half period's boundary; one time in four the walk's three words (+0x64, +0x78, +0x98) 0, a walk of 0
void steerHeadsRoot(Vc2OscVoice& o, std::mt19937& rng) {
    constexpr uint32_t kWalkWords[] = { 0x64, 0x78, 0x98 };   // [FW] the walk target's (0x893afc30)
    constexpr float kLeast = 1e-30f, kMost = 1e30f, kLow = 8.0f, kSpan = 14000.0f, kRate = 44100.0f;
    constexpr uint32_t kHalves = 4096, kShifts = 4;
    if (!(o.tail.hz > kLeast && o.tail.hz < kMost)) o.tail.hz = kLow + kSpan * vc2Unit(rng);
    if (rng() % 2) {
        const uint32_t j = 1 + (uint32_t)rng() % kShifts;
        o.tail.hz = kRate / (float)(1u << j);
        o.setU32(kLiteFrames, (1 + (uint32_t)rng() % kHalves) << j);
    }
    if (rng() % 4 == 0) {
        for (uint32_t at : kWalkWords) o.setF32(at, 0.0f);
    }
}

// v240: LITE's note-on (kind 1) on the voice's words: one time in two the first sample and the frames (+0x14c,
// +0x150) a wave's, below 2^17, where a sample more or less shows as a float, and the first 0 (type 1's range in
// events); each seek's word (+0x154, +0x158) the first's (+0x14c, +0x150), so no seek; one time in three its two
// heads (the wave's type 2), steered as above
void steerLite(OscSides& s, std::mt19937& rng, int v) {
    constexpr uint32_t kSeekTo = 0x154, kSeekToOne = 0x158, kWaveFrames = 1u << 17;   // [FW]
    Vc2OscVoice& o = s.voices[(size_t)v];
    if (o.kind() != 1) return;
    if (rng() % 2) o.setU32(kLiteFrames, (uint32_t)rng() % kWaveFrames);
    if (rng() % 2) o.setU32(kLiteFirst, rng() % 2 ? 0 : (uint32_t)rng() % kWaveFrames);
    if (rng() % 2) o.setU32(kSeekTo, o.u32(kLiteFirst));
    if (rng() % 2) o.setU32(kSeekToOne, o.u32(kLiteFrames));
    if (rng() % 3 == 0) o.setU32(Vc2OscVoice::kWave, 2);
    if (o.u32(Vc2OscVoice::kWave) == 2) steerHeadsRoot(o, rng);
    oscWriteVoice(s, v);
}
} // namespace

std::array<std::pair<uint32_t, uint32_t>, 12> oscAnalogWords(std::mt19937& rng) {
    constexpr uint32_t kCodes = 11, kPast = 8, kToneKfs = 15;   // [FW] the codes 0…10; the Tone KF table's 15
    constexpr std::array<float, 6> kSubOn { 1.0f, 2.0f, -1.0f, 0.99f, 1e9f, -0.0f };
    const uint32_t code = rng() % kPast == 0 ? kCodes + (uint32_t)rng() % kPast : (uint32_t)rng() % kCodes;
    const float subOn = rng() % 4 == 0 ? kSubOn[rng() % kSubOn.size()] : 0.0f;
    const float toneKf = (float)(rng() % kToneKfs) + 0.9f * vc2Unit(rng);
    // Impact as the converter gives it, Impact·4/40: its tenths 0…4, 1 (the default) one time in four; else moderate
    constexpr uint32_t kTenths = 41;
    const float tenths = (float)(rng() % kTenths) / 10.0f;
    const float impact = rng() % 4 == 0 ? 1.0f : rng() % 2 ? tenths : oscDrawFloat(rng, OscRange::moderate);
    std::array<std::pair<uint32_t, uint32_t>, 12> words { { { 0x104, code }, { 0x138, std::bit_cast<uint32_t>(subOn) },
                                                           { 0x144, std::bit_cast<uint32_t>(toneKf) },
                                                           { 0x130, std::bit_cast<uint32_t>(impact) } } };
    constexpr std::array<uint32_t, 8> kModerate { 0x108, 0x10c, 0x114, 0x11c, 0x120, 0x124, 0x134, 0x140 };
    for (size_t k = 0; k < kModerate.size(); ++k) {
        words[4 + k] = { kModerate[k], std::bit_cast<uint32_t>(oscDrawFloat(rng, OscRange::moderate)) };
    }
    return words;
}

void oscAnalogObject(Vc2OscVoice& o, std::mt19937& rng) {
    // [FW] Fat's and the Pulse Width's targets, the phase, the second wave's Pulse Width target, the level's target,
    // RAMP's last segment, the two words nothing writes, LA's steepness targets, SQUARE's and JUNO's
    constexpr std::array<uint32_t, 12> kWords { 0x24c, 0x254, 0x260, 0x278, 0x28c, 0x294, 0x298, 0x2a0, 0x2b0, 0x2b8,
                                                0x2c0, 0x2c8 };
    for (uint32_t at : kWords) o.setF32(at, oscDrawFloat(rng, OscRange::moderate));
}

namespace {
// v242: one time in two a pitch within an octave of the root, where SQUARE's steps part: the pitch's word (+0x5c), its
// envelope's and vibrato's depths (+0x70, +0xfc) 0, its factor (+0x60) and the rate's ratio 1, the offset and the
// pitch ramp's value 0
void steerNearRoot(Vc2OscVoice& o, std::mt19937& rng) {
    constexpr uint32_t kPitch = 0x5c, kFactor = 0x60;   // [FW] the cents' and the ratio's words (Vc2OscTargets)
    constexpr float kOctave = 1200.0f;
    o.setF32(kPitch, kOctave * (2.0f * vc2Unit(rng) - 1.0f));
    for (uint32_t at : { 0x70u, 0xfcu }) o.setF32(at, 0.0f);
    o.setF32(kFactor, 1.0f);
    o.tail.rateRatio = 1.0f;
    o.tail.offset = 0.0f;
    o.ramps[Vc2OscVoice::kPitchRamp].w.value = 0.0f;
}

// v242: kind 3's, the analog's, block words drawn; a pitch near the root half the time, then half of those SQUARE
// (code 3), whose steps it parts; the object's words moderate (oscAnalogObject); LA's codes (9, 10) half the time
// Impact below 1 and +0x298, which the phases' terms are held against, large, where the phase passes 1 (v244); NOISE's
// (code 8) Pulse Width one time in four a NaN, where its hold meets 0 (FTRC of a NaN: 0 on both sides as a byte; a NaN
// alone in each product).  Not on the other codes: their sums take it through xtf::sub, whose NaN's sign is the
// interpreter's FSUB's negated
void steerAnalog(OscSides& s, std::mt19937& rng, int v) {
    constexpr uint32_t kAnalog = 3, kPw = 0x11c, kCode = 0x104, kSquare = 3, kNoise = 8, kLaFirst = 9;   // [FW]
    constexpr uint32_t kImpact = 0x130, kUnwritten = 0x298;                                            // [FW]
    Vc2OscVoice& o = s.voices[(size_t)v];
    if (o.kind() != kAnalog) return;
    for (const auto& [at, bits] : oscAnalogWords(rng)) o.setU32(at, bits);
    if (rng() % 2) {
        steerNearRoot(o, rng);
        if (rng() % 2) o.setU32(kCode, kSquare);
    }
    oscAnalogObject(o, rng);
    if (o.u32(kCode) - kLaFirst < 2 && rng() % 2) {
        o.setF32(kImpact, 0.5f * vc2Unit(rng));
        o.setF32(kUnwritten, 2.0f + 100.0f * vc2Unit(rng));
    }
    if (o.u32(kCode) == kNoise && rng() % 4 == 0) o.setF32(kPw, NAN);
    oscWriteVoice(s, v);
}
} // namespace

// v244: a third each the players' kinds (oscSteerPlayers), the analog (kind 3) and the kind as the event finds it;
// then LITE's and the analog's own draws by the kind
std::string oscOnKindNoteOn(OscSides& s, std::mt19937& rng, int v) {
    constexpr uint32_t kAnalog = 3;
    const uint32_t pick = rng() % 3;
    if (pick == 0) oscSteerPlayers(s, rng, v);
    if (pick == 1) s.voices[(size_t)v].setU32(Vc2OscVoice::kKind, kAnalog);
    steerLite(s, rng, v);
    steerAnalog(s, rng, v);
    ++s.n.noteOns;
    return oscBothSides(s, kKindNoteOn, { oscVoiceAt(v), (uint32_t)rng() }, OscAnswer::none, [&] {
        s.voices[(size_t)v].kindNoteOn(s.parts[(size_t)v], s.romShared);
        return 0u;
    });
}
#endif
