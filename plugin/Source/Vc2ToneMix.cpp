// The VC-2 tone's OSC mix, its object: see Vc2ToneMix.h (v269).  The renders are Vc2ToneMixRender.cpp.
#include "Vc2ToneMix.h"
#include "Vc2ToneMixRoutines.h"
#include "Vc2ToneMixTables.h"
#include "XtFloat.h"
#include <algorithm>
#include <bit>
#include <cstring>

namespace {
constexpr uint32_t kBlockBytes = 5 * sizeof(uint32_t);   // [FW] 0x893adbf0 copies the block to +0x00…+0x13
constexpr uint32_t kNoVirtual = 0xffffffff;              // [FW] the member pointers' second word, at 0x8993ea64
constexpr int kKeepsGain = -1;                           // [FW] a type past the table: 0x893adff0 leaves the gain

// A render's number and its output gain's, as the install chooses them
struct MixPick {
    int routine, gain;
};

using MixStart = void (*)(Vc2MixWords&, Vc2ToneTail&, Vc2ToneMixRings&);

void startFour(Vc2MixWords& w, Vc2ToneTail&, Vc2ToneMixRings&) { w.gain = w.target; }

void startRing(Vc2MixWords& w, Vc2ToneTail&, Vc2ToneMixRings&) {
    w.target[0] = xtf::div(w.target[0], vc2ToneMixTables().fullScale);
    std::copy_n(w.target.begin(), 3, w.gain.begin());
}

void startDelay(Vc2MixWords& w, Vc2ToneTail&, Vc2ToneMixRings& rings) {
    std::copy_n(w.target.begin(), 3, w.gain.begin());
    rings.a(w.ringA).fill(0.0f);
    rings.b(w.ringB).fill(0.0f);
}

void startFollow(Vc2MixWords& w, Vc2ToneTail&, Vc2ToneMixRings&) {
    w.gain[0] = w.target[2];
    w.hold = 0.0f;
}

void startBlend(Vc2MixWords& w, Vc2ToneTail& tail, Vc2ToneMixRings&) {
    tail.syncOn = 1;
    w.gain[0] = w.gain[1] = 0.0f;
    w.hold = w.target[0];
}

void startOther(Vc2MixWords&, Vc2ToneTail& tail, Vc2ToneMixRings&) { tail.syncOn = 0; }

// [FW] a block's type: its note-on's case (0x893adc86…) and its install's (0x893adeb6…): the pick, and the other pick
// where the OSC voices' flags are set or, for the last type, where OSC 1 isn't kind 3
struct MixType {
    MixStart start;
    MixPick pick, other;
    bool byKind;
};
constexpr MixType kTypes[] = {
    { startFour, { 0, 0 }, { 0, 0 }, false },     { startRing, { 1, 1 }, { 1, 1 }, false },
    { startDelay, { 2, 2 }, { 3, 4 }, false },    { startFollow, { 4, 3 }, { 5, 5 }, false },
    { startBlend, { 6, 6 }, { 7, 7 }, true },
};
constexpr MixType kOtherType = { startOther, { 0, kKeepsGain }, { 0, kKeepsGain }, false };

const MixType& typeOf(int32_t type) {
    return (uint32_t)type < std::size(kTypes) ? kTypes[(size_t)type] : kOtherType;
}

// [FW] which of the OSC voices' flags are set (0x893adc0a…0x893adc28)
int32_t flagsOf(uint32_t flag1, uint32_t flag2) {
    if (flag1 != 0) return flag2 != 0 ? 1 : 2;
    return flag2 != 0 ? 3 : 0;
}
} // namespace

void Vc2ToneMix::construct(Vc2ToneTail& tail) {
    tail.syncOn = tail.sync = 0;
    tail.unread[kVc2MixGainWord] = 0;
}

void Vc2ToneMix::noteOn(const uint32_t* block, uint32_t flag1, uint32_t flag2, Vc2ToneTail& tail,
                        Vc2ToneMixRings& rings) {
    std::memcpy(&w.type, block, kBlockBytes);
    w.typeByte[0] = (uint8_t)block[0];
    w.flags = flagsOf(flag1, flag2);
    const uint32_t offset = (uint32_t)tail.index * Vc2ToneMixRings::kBytes;
    w.ringA = Vc2ToneMixRings::kAAt + offset;
    w.ringB = Vc2ToneMixRings::kBAt + offset;
    typeOf(w.type).start(w, tail, rings);
}

void Vc2ToneMix::install(bool kind3, Vc2ToneTail& tail) {
    const MixType& type = typeOf(w.type);
    const MixPick pick = (type.byKind ? !kind3 : w.flags != 0) ? type.other : type.pick;
    tail.unread[0] = 0;
    tail.unread[1] = kNoVirtual;
    tail.unread[kVc2MixRoutineWord] = kVc2MixRoutines[(size_t)pick.routine];
    if (pick.gain != kKeepsGain) {
        tail.unread[kVc2MixGainWord] = std::bit_cast<uint32_t>(vc2ToneMixTables().gains[(size_t)pick.gain]);
    }
}
