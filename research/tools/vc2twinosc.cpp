// vc2run osc <rom.2>: the OSC block's routines that rom.4 shares, on rom.2's and rom.4's, without the port (xt_osc holds
// the port to rom.2, so rom.2 equal to rom.4 is the port equal to rom.4), over xt_osc's inputs (plugin/Source/XtOsc.cpp):
//   - return for return: exp2 (0x894ec090 / 0x893a8340), powf at 2 (0x8952ff9c / 0x89506e54, the converter's powf in
//     rom.4), the velocity curve (0x894ebd60 / 0x893a7fb0) and sensitivity (0x894ebe20 / 0x893a8070), pitch10
//     (0x894e1250 / 0x8939c8a0) and the key-follow key (0x894e12c0 / 0x8939c910);
//   - the pitch ramp, value for value: its constructor (0x894f4860 / 0x893b0dc0, longer in rom.4), start (0x894f4890 /
//     0x893b0e10), legato update (0x894f48b0 / 0x893b0e30), restart (0x894f4920 / 0x893b0b10) and tick (0x894f4b00 /
//     0x893b0f30), both rewritten in rom.4 and placed by codealign.py, and value (0x894f4c00 / 0x893b1160);
//   - the mono/poly key state (reset 0x894d6420 / 0x893923c0, note-on 0x894d6330 / 0x893922d0, note-off 0x894d63c0 /
//     0x89392360), the system clock hooked in each (0x89301940 / 0x893018d0): the object after every event.
// Fails on any difference.
#include "vc2runs.h"
#include "vc2twin.h"
#include <array>
#include <cstdio>
#include <cstring>
#include <vector>

namespace vc2 {

namespace {

using Pair = Twin::Pair;
using Reg = Twin::Reg;
constexpr Pair kExp2{0x894ec090, 0x893a8340}, kPowf{0x8952ff9c, 0x89506e54};             // [FW]
constexpr Pair kVelCurve{0x894ebd60, 0x893a7fb0}, kVelSens{0x894ebe20, 0x893a8070};      // [FW]
constexpr Pair kPitch10{0x894e1250, 0x8939c8a0}, kKfKey{0x894e12c0, 0x8939c910};         // [FW]
constexpr Pair kRampCtor{0x894f4860, 0x893b0dc0}, kRampStart{0x894f4890, 0x893b0e10};     // [FW]
constexpr Pair kRampUpdate{0x894f48b0, 0x893b0e30}, kRampRestart{0x894f4920, 0x893b0b10};  // [FW]
constexpr Pair kRampTick{0x894f4b00, 0x893b0f30}, kRampValue{0x894f4c00, 0x893b1160};     // [FW]
constexpr Pair kKeyReset{0x894d6420, 0x893923c0}, kKeyOn{0x894d6330, 0x893922d0};         // [FW]
constexpr Pair kKeyOff{0x894d63c0, 0x89392360}, kClock{0x89301940, 0x893018d0};           // [FW]

constexpr uint32_t kRamp = kScratch, kRampSettings = kScratch + 0x100;
constexpr uint32_t kKeys = kScratch + 0x200, kKeysSize = 0x24, kChannel = kScratch + 0x300, kChannelSize = 0x40;
constexpr uint32_t kCentreKey = 60;
constexpr int kKfs[] = {-2000, -1500, -1000, -990, -10, 0, 10, 500, 990, 1000, 1010, 2000};

// The routines rom.4 shares, return for return: false on a fault
bool returnsCases(Twin& t, Twin::Returns& r) {
    for (int i = -60000; i <= 60000; i += 7)
        if (!t.returns(kExp2, {}, Reg::fr0, r, (float)i / 10007.0f)) return false;
    uint32_t x = 1;
    for (int i = 0; i < 200000; ++i) {
        x = x * 1664525u + 1013904223u;
        const float scale = i % 3 == 0 ? 7.0f : (i % 3 == 1 ? 1.0f : 0.01f);
        if (!t.returns(kPowf, {}, Reg::fr0, r, 2.0f, (float)(int32_t)x * 0x1p-31f * scale)) return false;
    }
    for (uint32_t curve = 0; curve <= 8; ++curve)
        for (int sens = -210; sens <= 210; ++sens)
            for (uint32_t vel = 0; vel < 128; ++vel)
                if (!t.returns(kVelCurve, {vel, (uint32_t)sens, curve}, Reg::r0, r)) return false;
    for (int s = -100; s <= 100; ++s)
        if (!t.returns(kVelSens, {(uint32_t)s}, Reg::r0, r)) return false;
    t.write(kStack, &kCentreKey, 4);   // pitch10's fifth argument, on the stack
    for (uint32_t key = 0; key < 128; ++key)
        for (int coarse = -60; coarse <= 60; coarse += 4)
            for (const int kf : kKfs) {
                for (const int fine : {-500, -37, 0, 1, 499}) {
                    const std::initializer_list<uint32_t> args{key, (uint32_t)coarse, (uint32_t)kf, (uint32_t)fine};
                    if (!t.returns(kPitch10, args, Reg::r0, r)) return false;
                }
                if (!t.returns(kKfKey, {key, (uint32_t)coarse, (uint32_t)kf, kCentreKey}, Reg::r0, r)) return false;
            }
    return true;
}

// The portamento settings a ramp is given: on, its time in ms, its type
// The ramp's values tallied by what last set it going (a start, a legato update, an update and a restart) and the
// portamento type it was given, so a difference says where it is; the first few said with the settings
struct RampTally {
    static constexpr const char* kWhat[] = {"start", "legato update", "update and restart"};
    static constexpr long kSaidEvery = 1500;   // one difference in so many is said
    std::array<Twin::Returns, 6> by{};
    uint32_t last = 0, type = 0, on = 0;
    int op = 0;                                // the operation in this ramp's sequence that last set it going
    float ms = 0, from = 0, to = 0;
    long seen = 0;
    std::array<long, 2> byFirst{};             // differences after the ramp's first operation, and after a later one
    Twin::Returns gliding;                     // the values while rom.2's glide is under way (its value not 0)
    Twin::Returns& now() { return by[last * 2 + type]; }
};

void writeRampSettings(Twin& t, Rng& r, RampTally& v) {
    std::vector<uint32_t> s(8, 0);
    v.on = r.next() % 5 != 0 ? 1 : 0;
    v.ms = r.next() % 6 == 0 ? 0.0f : r.in(1.0f, 4000.0f);
    v.type = r.next() & 1;
    s[0] = v.on;
    std::memcpy(&s[3], &v.ms, 4);
    s[4] = v.type;
    t.write(kRampSettings, s.data(), s.size() * 4);
}

// A ramp value compared, a difference said while there are few
bool rampValue(Twin& t, RampTally& v) {
    const long before = v.now().differ;
    if (!t.returns(kRampValue, {kRamp}, Reg::fr0, v.now())) return false;
    const bool differs = v.now().differ != before;
    if ((t.vsynth.fb[0][0] & 0x7fffffffu) != 0) {
        ++v.gliding.cases;
        v.gliding.differ += differs ? 1 : 0;
    }
    if (!differs) return true;
    ++v.byFirst[v.op == 0 ? 0 : 1];
    if (v.seen++ % RampTally::kSaidEvery != 0) return true;
    float a = 0, b = 0;
    std::memcpy(&a, &t.vsynth.fb[0][0], 4);
    std::memcpy(&b, &t.vc2.cpu.fb[0][0], 4);
    std::printf("ramp after a %s at op %d (on %u type %u, %g ms, %g to %g cents): rom.2 %.9g, rom.4 %.9g\n",
                RampTally::kWhat[v.last], v.op, v.on, v.type, v.ms, v.from, v.to, a, b);
    return true;
}

float cents(Rng& r) {
    if (r.next() % 7 == 0) return 0.0f;
    const float c = (float)((int)(r.next() % 12001) - 6000);
    return r.next() % 3 == 0 ? c : c * 0.1f;
}

// One ramp through starts, legato updates with and without a restart, and ticks, its value compared after each tick
bool rampCase(Twin& t, Rng& r, RampTally& v) {
    const std::vector<uint8_t> zero(0x60, 0);
    t.write(kRamp, zero.data(), zero.size());
    if (!t.call(kRampCtor, {kRamp, 0})) return false;
    for (int op = 0; op < 60; ++op) {
        const uint32_t what = op == 0 ? 0 : r.next() % 8;
        if (what <= 1) {
            writeRampSettings(t, r, v);
            v.last = 0;
            v.op = op;
            v.from = cents(r);
            v.to = cents(r);
            if (!t.call(kRampStart, {kRamp, kRampSettings}, v.from, v.to)) return false;
        } else if (what <= 3) {
            writeRampSettings(t, r, v);
            v.last = what == 2 ? 2 : 1;
            v.op = op;
            if (!t.call(kRampUpdate, {kRamp, kRampSettings})) return false;
            v.from = cents(r);
            v.to = cents(r);
            if (what == 2 && !t.call(kRampRestart, {kRamp}, v.from, v.to)) return false;
        } else {
            for (uint32_t k = r.next() % 30; k > 0; --k)
                if (!t.call(kRampTick, {kRamp, 1}) || !rampValue(t, v)) return false;
        }
    }
    return true;
}

// One key event on the key state: the channel's held keys and the key written to both, then the note-on or note-off
bool keyEvent(Twin& t, uint16_t (&held)[8], uint32_t key, bool on) {
    const uint16_t bit = (uint16_t)(1u << (key & 15));
    held[key >> 4] = on ? (uint16_t)(held[key >> 4] | bit) : (uint16_t)(held[key >> 4] & ~bit);
    t.write(kChannel + 0xc + 2 * (key >> 4), &held[key >> 4], 2);
    const uint8_t k8 = (uint8_t)key;
    t.write(kChannel + (on ? 7 : 9), &k8, 1);
    return on ? t.call(kKeyOn, {kKeys}) : t.call(kKeyOff, {kKeys, 0, 0x7f});
}

// One sequence of key events, the object and channel compared after each: false on a fault or a difference, said
bool keyCase(Twin& t, Rng& r, int32_t& nowMs, long& events) {
    const std::vector<uint8_t> zero(kChannelSize, 0);
    t.write(kKeys, zero.data(), kKeysSize);
    t.write(kChannel, zero.data(), kChannelSize);
    t.write(kKeys, &kChannel, 4);
    if (!t.call(kKeyReset, {kKeys})) return false;
    nowMs = (int32_t)(r.next() % 100000);
    uint16_t held[8] = {};
    const uint32_t span = 1 + (r.next() % 3) * 20;
    for (int e = 0; e < 300; ++e, ++events) {
        nowMs += (int32_t)(r.next() % (r.next() % 2 ? 60 : 3000));
        const uint32_t key = 40 + r.next() % span;
        const bool on = !(held[key >> 4] & (1u << (key & 15))) && r.next() % 3 != 0;
        if (!keyEvent(t, held, key, on)) return false;
        long at = t.differs(kKeys, kKeysSize);
        const uint32_t base = at >= 0 ? kKeys : kChannel;
        if (at < 0) at = t.differs(kChannel, kChannelSize);
        if (at < 0 && (on || t.vsynth.r[0] == t.vc2.cpu.r[0])) continue;
        std::printf("key state, %s %u at %d ms: ", on ? "on" : "off", key, nowMs);
        if (at >= 0) t.sayDiffers(base, at);
        std::printf("%s\n", at >= 0 ? "" : "note-off's return differs");
        return false;
    }
    return true;
}

int oscCases(Twin& t) {
    Twin::Returns ret, ramp;
    if (!returnsCases(t, ret)) return -1;
    Rng r;
    RampTally tally;
    for (int g = 0; g < 4000; ++g)
        if (!rampCase(t, r, tally)) return -1;
    for (size_t i = 0; i < tally.by.size(); ++i) {
        std::printf("ramp after a %s, type %zu: %ld values, %ld different\n", RampTally::kWhat[i / 2], i % 2,
                    tally.by[i].cases, tally.by[i].differ);
        ramp.cases += tally.by[i].cases;
        ramp.differ += tally.by[i].differ;
    }
    std::printf("ramp differences after the ramp's first operation %ld, after a later one %ld; while rom.2 glides "
                "%ld values, %ld different\n", tally.byFirst[0], tally.byFirst[1], tally.gliding.cases,
                tally.gliding.differ);
    int32_t nowMs = 0;
    t.hook(kClock, [&nowMs](Sh4& c) { c.ret((uint32_t)nowMs); });
    long events = 0;
    int keyBad = 0;
    for (int seq = 0; seq < 400; ++seq) keyBad += keyCase(t, r, nowMs, events) ? 0 : 1;
    std::printf("osc: returns %ld, %ld different; ramp values %ld, %ld different; key state %ld events, %d sequences "
                "different\n", ret.cases, ret.differ, ramp.cases, ramp.differ, events, keyBad);
    return (int)(ret.differ + ramp.differ) + keyBad;
}

} // namespace

int osc(Machine& m, int argc, char** argv) {
    return twinRun(m, argc, argv, noPortTables, oscCases);
}

} // namespace vc2
