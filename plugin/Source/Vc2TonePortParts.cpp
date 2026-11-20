// xt_vc2tone's port side: a tone's parts, see Vc2ToneCheck.h (v228, v229).  From v316 the parts the port has whole are
// the plugin's host's, Vc2ToneHost, which these derive from; from v319 every part.  What is left here is the tone's own
// calls beyond its parts, each logged as rom.4's hook reads it (Vc2ToneFakes.cpp), and what its OSC voices read beyond
// the tone, xt_vc2osc's fake parts, each on its voice, tone × 2 + its OSC.  XT_VC2TONE_MAIN only.
#ifdef XT_VC2TONE_MAIN
#include "Vc2ToneCheck.h"

namespace {
constexpr uint32_t kFadeEvent = 0x1000;   // [FW] 0x893b3be4: the event the truncation's task waits on
} // namespace

TonePortParts::TonePortParts(int tone, ToneShared& shared)
    : Vc2ToneHost(tone, { *shared.pool, shared.lists, shared.rings, shared.frees, shared.chaos, shared.fadeStep,
                          shared.pools, shared.past.data(), *shared.math }),
      s_(shared),
      outside_ { { OscPortParts(2 * tone, shared.osc, oscs[0]), OscPortParts(2 * tone + 1, shared.osc, oscs[1]) } } {}

const Vc2ToneHost& TonePortParts::hostOf(int tone) { return (*s_.parts)[(size_t)tone]; }

void TonePortParts::osEvent(uint32_t event, uint32_t flags) {
    log(kOsEvent, kToneNone, { event, flags }, false);
}

void TonePortParts::followLfo(int lfo, int tone, const uint32_t* block) {
    ++s_.follows;
    Vc2ToneHost::followLfo(lfo, tone, block);
}

uint32_t TonePortParts::freeLfo(uint32_t id, int lfo) {
    ++s_.takes;
    return Vc2ToneHost::freeLfo(id, lfo);
}

void TonePortParts::takePlayer(int osc, int tone) {
    ++s_.players;
    Vc2ToneHost::takePlayer(osc, tone);
}

void TonePortParts::ended() {
    log(kEnded, kToneNone, { s_.pool ? (uint32_t)s_.pool->tones[(size_t)tone()].head.state : 0u });
}

// A wait the truncation's task makes; the TVA's fade at 0 once the draws' count of them has passed, as rom.4's hook
// leaves it, the block task's renders standing in
void TonePortParts::waitFade() {
    log(kWaitFade, kToneNone, { kFadeEvent }, false);
    if (--s_.fade.portLeft <= 0) tva.w.fade = 0.0f;
}

void TonePortParts::truncated() { log(kTruncated, kToneNone); }
#endif
