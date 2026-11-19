// xt_vc2tone's port side: a tone's fake parts, see Vc2ToneCheck.h (v228, v229).  Each call logged as rom.4's hook reads
// it (Vc2ToneFakes.cpp), the answers from the shared draws and the tone's words, what the note-on binds set in its
// words.  XT_VC2TONE_MAIN only.
#ifdef XT_VC2TONE_MAIN
#include "Vc2ToneCheck.h"
#include <algorithm>
#include <utility>

namespace {
using Piece = Vc2TonePiece;
constexpr uint32_t kNoBuffer = 0xff, kByte = 0xff;
uint32_t bits(float x) { return std::bit_cast<uint32_t>(x); }
int32_t oscCode(int osc) { return tonePartCode(static_cast<Piece>((int)Piece::osc1 + osc), 0); }
int32_t cosmCode(int cosm) { return tonePartCode(static_cast<Piece>((int)Piece::cosm1 + cosm), 0); }
int32_t lfoCode(int lfo) { return tonePartCode(Piece::lfos, lfo); }
int32_t envelopeCode(int envelope) { return tonePartCode(Piece::envelopes, envelope); }
const int32_t kTvaCode = tonePartCode(Piece::tva, 0);
constexpr uint32_t kFadeEvent = 0x1000;   // [FW] 0x893b3be4: the event the truncation's task waits on
uint32_t byteOf(int32_t x) { return (uint32_t)x & kByte; }
} // namespace

void TonePortParts::build(Vc2TonePiece piece) {
    if (piece == Piece::envelopes) mods.buildEnvelopes();
    else if (piece == Piece::lfos) mods.buildLfos(s_.chaos);
    else if (piece == Piece::mix) mix.construct(tail());
    else log(kBuild, tonePartCode(piece, 0));
}

void TonePortParts::destroy(Vc2TonePiece piece) {
    if (piece == Piece::lfos) mods.destroyLfos();
    else log(kDestroy, tonePartCode(piece, 0));
}

void TonePortParts::unfollow(int lfo) { s_.frees.release(mods.lfos[(size_t)lfo].w.follow); }
void TonePortParts::unlist(bool fromOne) {
    const uint32_t self = s_.pool->addressOf(tone_);
    if (fromOne) s_.lists.unlistNew(self, *s_.pool);
    else s_.lists.unlistSounding(self, *s_.pool);
}

void TonePortParts::tickEnvelope(int envelope, int32_t n) { mods.envelopes[(size_t)envelope].tick(n); }
void TonePortParts::tickLfo(int lfo, int32_t n) { mods.lfos[(size_t)lfo].tick(n); }
void TonePortParts::clockLfo(int lfo, float x) { mods.lfos[(size_t)lfo].clock(x); }
void TonePortParts::updateOsc(int osc, int32_t n) { log(kUpdateOsc, oscCode(osc), { (uint32_t)n }); }
void TonePortParts::updateCosm(int cosm, float x) { log(kUpdateCosm, cosmCode(cosm), { bits(x) }); }
void TonePortParts::updateTva() { log(kUpdateTva, kTvaCode); }

void TonePortParts::ended() {
    log(kEnded, kToneNone, { s_.pool ? (uint32_t)s_.pool->tones[(size_t)tone_].head.state : 0u });
}

void TonePortParts::renderOsc(int osc, float* buffer) { log(kRenderOsc, oscCode(osc), { bufferOf(buffer) }); }

void TonePortParts::renderMix(const float* a, const float* b, float* c) { mix.render(a, b, c, tail(), s_.rings); }

void TonePortParts::renderCosm(int cosm, float* buffer) { log(kRenderCosm, cosmCode(cosm), { bufferOf(buffer) }); }

void TonePortParts::renderTva(const float* in, float* out) {
    log(kRenderTva, kTvaCode, { bufferOf(in) | bufferOf(out) << 8 });
}

void TonePortParts::countOsc(int osc, uint32_t x) { log(kCountOsc, oscCode(osc), { x }); }
void TonePortParts::countOscMatched(int osc, uint32_t x) { log(kCountOscMatched, oscCode(osc), { x }); }

uint32_t TonePortParts::installOsc(int osc) {
    log(kInstallOsc, oscCode(osc));
    return s_.draws.installOsc[(size_t)osc];
}

void TonePortParts::installMix(bool kind3) { mix.install(kind3, tail()); }

uint32_t TonePortParts::installCosm(int cosm, uint32_t flag) {
    log(kInstallCosm, cosmCode(cosm), { flag });
    return s_.draws.installCosm[(size_t)cosm];
}

void TonePortParts::noteOnEnvelope(int envelope, const uint32_t* block) {
    mods.envelopes[(size_t)envelope].noteOn(block);
}

void TonePortParts::noteOnLfo(int lfo, const uint32_t* block, float x) { mods.lfos[(size_t)lfo].noteOn(block, x); }

void TonePortParts::followLfo(int lfo, int tone, const uint32_t* block) {
    ++s_.follows;
    const Vc2ToneLfo& other = (*s_.parts)[(size_t)tone].mods.lfos[(size_t)lfo];
    mods.lfos[(size_t)lfo].follow(other, block, s_.frees);
}

uint32_t TonePortParts::freeLfo(uint32_t id, int lfo) {
    ++s_.takes;
    return s_.frees.take(id, lfo);
}

void TonePortParts::attachFree(int lfo, uint32_t free) { mods.lfos[(size_t)lfo].attach(free, s_.frees); }
float TonePortParts::freeValue(uint32_t free) { return s_.frees.object(free).phase; }

void TonePortParts::startFree(int lfo, const uint32_t* block, float, float x) {
    mods.lfos[(size_t)lfo].startFree(block, x, s_.frees);
}

void TonePortParts::relist(uint32_t follow, uint32_t id, int lfo) { s_.frees.addClock(follow, id, lfo); }
void TonePortParts::list(uint32_t id, int lfo) { s_.frees.removeClock(id, lfo); }

// The free object's +0 (the LFO's number) and +4 (the id), as the note-on leaves them
void TonePortParts::markFree(uint32_t free, uint32_t id, int lfo) {
    Vc2FreeLfoWords& o = s_.frees.object(free);
    o.lfo = lfo;
    o.id = id;
}

// The bound envelopes on (+0x68 1) and LFOs not (+0x80 0), as each binding leaves them
void TonePortParts::setFlags(std::span<const int32_t> envelopes, std::span<const int32_t> lfos) {
    for (int32_t e : envelopes) mods.envelopes[(size_t)e].w.on = 1;
    for (int32_t l : lfos) mods.lfos[(size_t)l].w.on = 0;
}

void TonePortParts::bindOsc(int osc, const Vc2ToneBinding<4>& b) {
    const uint32_t self = s_.pool->addressOf(tone_);
    auto& binds = words.osc[(size_t)osc].binds;
    std::ranges::transform(b.envelopes, binds.begin(), [&](int32_t e) { return toneEnvelopeAt(self, e); });
    std::ranges::transform(b.lfos, binds.begin() + 4, [&](int32_t l) { return toneLfoAt(self, l); });
    setFlags(b.envelopes, b.lfos);
}

void TonePortParts::bindCosm(int cosm, const Vc2ToneBinding<2>& b) {
    const uint32_t numbers = byteOf(b.envelopes[0]) | byteOf(b.envelopes[1]) << 8 | byteOf(b.lfos[0]) << 16
                             | byteOf(b.lfos[1]) << 24;
    log(kBindCosm, cosmCode(cosm), { numbers });
    setFlags(b.envelopes, b.lfos);
}

void TonePortParts::bindTva(int envelope, int lfoA, int lfoB, int32_t serial) {
    const uint32_t self = s_.pool->addressOf(tone_);
    words.tvaBinds = { toneEnvelopeAt(self, envelope), toneLfoAt(self, lfoA), toneLfoAt(self, lfoB), (uint32_t)serial };
    const int32_t envelopes[] = { envelope }, lfos[] = { lfoA, lfoB };
    setFlags(envelopes, lfos);
}

void TonePortParts::noteOnOsc(int osc, const uint32_t* block, bool legato) {
    log(kNoteOnOsc, oscCode(osc), { blockOf(block), legato ? 1u : 0u });
}

void TonePortParts::takePlayer(int osc, int tone) {
    log(kTakePlayer, oscCode(osc), { (uint32_t)tone, (uint32_t)osc });
}

void TonePortParts::setOsc(int osc, const uint32_t* block) { log(kSetOsc, oscCode(osc), { blockOf(block) }); }

void TonePortParts::noteOnMix(const uint32_t* block, uint32_t flag1, uint32_t flag2) {
    mix.noteOn(block, flag1, flag2, tail(), s_.rings);
}

Vc2ToneCosmSetUp TonePortParts::cosmSetUp(int osc) {
    const ToneOscWords& o = words.osc[(size_t)osc];
    return { std::bit_cast<float>(o.value), std::bit_cast<float>(o.other), (uint8_t)(o.byte & kByte) };
}

void TonePortParts::setUpCosm(int cosm, const Vc2ToneCosmSetUp& s, uint32_t flag) {
    log(kSetUpCosm, cosmCode(cosm), { bits(s.value), bits(s.other), s.byte, flag });
}

void TonePortParts::startCosm(int cosm, const uint32_t* block, int which) {
    log(kStartCosm, cosmCode(cosm), { blockOf(block), (uint32_t)which });
}

void TonePortParts::noteOnTva(const uint32_t* block, uint32_t flag) {
    log(kNoteOnTva, kTvaCode, { blockOf(block), flag });
}

void TonePortParts::enlist() { s_.lists.enlist(s_.pool->addressOf(tone_), *s_.pool); }

// v273: the remaining methods' parts, each logged as rom.4's hook reads it but the mix's, which is real
void TonePortParts::settingsOsc(int osc, const uint32_t* block) { log(kSettingsOsc, oscCode(osc), { blockOf(block) }); }

void TonePortParts::settingsLfo(uint32_t lfo, const uint32_t* block) {
    log(kSettingsLfo, lfoCode((int)lfo), { blockOf(block) });
}

void TonePortParts::settingsEnvelope(uint32_t envelope, const uint32_t* block) {
    log(kSettingsEnvelope, envelopeCode((int)envelope), { blockOf(block) });
}

void TonePortParts::newBlockCosm(int cosm, const uint32_t* block) {
    log(kNewBlockCosm, cosmCode(cosm), { blockOf(block) });
}

void TonePortParts::settingsMix(const uint32_t* block) { mix.settings(block, tail()); }
void TonePortParts::newBlockTva(const uint32_t* block) { log(kNewBlockTva, kTvaCode, { blockOf(block) }); }
void TonePortParts::legatoOsc(int osc, const uint32_t* ramp) { log(kLegatoOsc, oscCode(osc), { blockOf(ramp) }); }
void TonePortParts::releaseLfo(int lfo) { log(kReleaseLfo, lfoCode(lfo)); }
void TonePortParts::releaseEnvelope(int envelope) { log(kReleaseEnvelope, envelopeCode(envelope)); }
void TonePortParts::keyOffOsc(int osc) { log(kKeyOffOsc, oscCode(osc)); }
void TonePortParts::truncateTvaTold() { log(kTruncateTold, kTvaCode); }
void TonePortParts::truncateTva() { log(kTruncate, kTvaCode); }

// A wait the truncation's task makes; the fade at 0 once the draws' count of them has passed, as rom.4's hook leaves it
void TonePortParts::waitFade() {
    log(kWaitFade, kToneNone, { kFadeEvent }, false);
    if (--s_.fade.portLeft <= 0) words.tvaFade = 0;
}

void TonePortParts::truncated() { log(kTruncated, kToneNone); }

int32_t TonePortParts::oscWord168(int osc) {
    log(kOscWord168, oscCode(osc));
    return (int32_t)s_.draws.word168[(size_t)osc];
}

uint32_t TonePortParts::oscPosition(int osc) {
    log(kOscPosition, oscCode(osc));
    return s_.draws.position[(size_t)osc];
}

bool TonePortParts::answered(ToneKind kind, bool answer) {
    log(kind, kTvaCode);
    return answer;
}

uint32_t TonePortParts::bufferOf(const float* p) const {
    const std::pair<const float*, uint32_t> buffers[] = {
        { s_.buffers.a, 1 }, { s_.buffers.b, 2 }, { s_.buffers.c, 3 }, { s_.out.data(), kToneOut },
    };
    const auto* found = std::ranges::find(buffers, p, &std::pair<const float*, uint32_t>::first);
    return found == std::end(buffers) ? kNoBuffer : found->second;
}

uint32_t TonePortParts::blockOf(const uint32_t* p) const {
    return (uint32_t)(p - s_.input.words.data()) * (uint32_t)sizeof(uint32_t);
}
#endif
