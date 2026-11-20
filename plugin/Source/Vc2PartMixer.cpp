// The VC-2's part mixer's levels: see Vc2PartMixer.h (v227).  The level routine, the targets, suspend, resume and the
// reset, each routine's arithmetic in its own order, in XtFloat; every address is rom.4's.  The block is
// Vc2MixerBlock.cpp.
#include "Vc2PartMixer.h"
#include "Vc2Math.h"
#include "Vc2PartMixerTables.h"
#include "XtFloat.h"

namespace {
using xtf::add;
using xtf::mul;

constexpr int32_t kExtin = 2;   // [FW] 0x893b14c0: OSC 1 Type's EXTIN

// The targets from the settings and the offsets, by a routine's pool: MULTI and MAIN their sum held to [0, 1] × 0.025,
// Reverb its send × MAIN's; MAIN's and Reverb's × 2^31 with the EXTIN flag
void targetsBy(Vc2MixerBuffers& b, const Vc2MixerOffsets& o, int32_t extin, const Vc2MixerTargetPool& pool) {
    const float multi = mul(vc2Clamp01(add(b.mfxSend, o.mfx)), pool.scale);
    const float main = mul(vc2Clamp01(add(b.directSound, o.direct)), pool.scale);
    const float reverb = mul(b.reverbSend, main);
    b.multiTarget = multi;
    b.mainTarget = main;
    b.reverbTarget = reverb;
    if (extin == 0) return;
    b.mainTarget = mul(main, pool.toBus);
    b.reverbTarget = mul(reverb, pool.toBus);
}
} // namespace

void Vc2PartMixer::levels(const Vc2MixerParams& p) {
    const Vc2PartMixerTables& t = vc2MixerTables();
    w.extin = p.oscType[0] == kExtin || p.oscType[1] == kExtin ? 1 : 0;
    b.directSound = mul(xtf::flt(p.direct), t.perLevel);
    b.reverbSend = mul(xtf::flt(p.reverbSend), t.perLevel);
    b.mfxSend = mul(xtf::flt(p.mfxSend), t.perLevel);
    if (bypass == 0) targetsBy(b, offsets, w.extin, t.pool[0]);
    b.main = b.mainTarget;   // MAIN and Reverb jump to their targets, MULTI eases
    b.reverb = b.reverbTarget;
}

void Vc2PartMixer::targets() {
    if (bypass == 0) targetsBy(b, offsets, w.extin, vc2MixerTables().pool[1]);
}

void Vc2PartMixer::suspend() {
    if (w.suspended != 0) return;
    w.suspended += 1;
    w.suspendMark = w.extin != 0 ? 1 : 0;
    bypass = 1;
    b.saved[0] = b.multiTarget;
    b.saved[1] = b.reverbTarget;
    b.saved[2] = b.mainTarget;
    b.multiTarget = b.reverbTarget = b.mainTarget = 0.0f;
}

void Vc2PartMixer::resume() {
    if (w.suspendMark < 0) return;
    w.suspended = 0;
    bypass = 0;
    w.suspendMark = -1;
    b.multiTarget = b.saved[0];
    b.reverbTarget = b.saved[1];
    b.mainTarget = b.saved[2];
}

void Vc2PartMixer::reset(Vc2MixerParts& parts) {
    parts.resetTracker();
    parts.resetUnvoiced();
    parts.resetRing();
}

namespace {
// The blocks since a routine's last run, its block kept (in rom.4's 32-bit arithmetic)
int32_t blocksSince(int32_t& at, int32_t blocks) {
    const auto d = static_cast<int32_t>(static_cast<uint32_t>(blocks) - static_cast<uint32_t>(at));
    at = blocks;
    return d;
}
} // namespace

void Vc2PartMixer::updateTones(std::span<Vc2MixerTone* const> tones, Vc2MixerTaskParts& parts) {
    const Vc2PartMixerTables& t = vc2MixerTables();
    const float ticks = xtf::div(mul(xtf::flt(blocksSince(w.updatedAt, w.blocks)), t.blockSeconds), t.updateSeconds);
    const int32_t n = ticks > 1.0f ? static_cast<int32_t>(ticks) : 1;
    for (Vc2MixerTone* tone : tones) tone->update(n);
    parts.glue(w.carried);
}

void Vc2PartMixer::scheduleTones(std::span<Vc2MixerTone* const> tones, Vc2MixerTaskParts& parts) {
    const Vc2PartMixerTables& t = vc2MixerTables();
    float seconds = mul(xtf::flt(blocksSince(w.scheduledAt, w.blocks)), t.blockSeconds);
    if (seconds == 0.0f) seconds = t.blockSeconds;
    parts.freeLfosClock(seconds);
    for (Vc2MixerTone* tone : tones) {
        tone->clock(seconds);
        tone->count(1);
    }
}
