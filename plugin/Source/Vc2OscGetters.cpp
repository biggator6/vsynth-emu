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
    oscWriteVoice(s, v);
}

std::string oscOnKindNoteOn(OscSides& s, std::mt19937& rng, int v) {
    if (rng() % 2) oscSteerPlayers(s, rng, v);
    ++s.n.noteOns;
    return oscBothSides(s, kKindNoteOn, { oscVoiceAt(v), (uint32_t)rng() }, OscAnswer::none, [&] {
        s.voices[(size_t)v].kindNoteOn(s.parts[(size_t)v], s.romShared);
        return 0u;
    });
}
#endif
