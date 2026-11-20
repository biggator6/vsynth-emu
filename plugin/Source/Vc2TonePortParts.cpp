// xt_vc2tone's port side: a tone's fake parts, see Vc2ToneCheck.h (v228, v229).  Each call logged as rom.4's hook reads
// it (Vc2ToneFakes.cpp), the answers from the shared draws and the tone's words, what the note-on binds set in its
// words.  From v316 the parts the port has whole are the plugin's host's, Vc2ToneHost, which these fakes derive from.
// XT_VC2TONE_MAIN only.
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
const int32_t kTvaCode = tonePartCode(Piece::tva, 0);
constexpr uint32_t kFadeEvent = 0x1000;   // [FW] 0x893b3be4: the event the truncation's task waits on
uint32_t byteOf(int32_t x) { return (uint32_t)x & kByte; }
} // namespace

TonePortParts::TonePortParts(int tone, ToneShared& shared)
    : Vc2ToneHost(tone, { *shared.pool, shared.lists, shared.rings, shared.frees, shared.chaos }), s_(shared) {}

void TonePortParts::buildPart(Vc2TonePiece piece) { log(kBuild, tonePartCode(piece, 0)); }
void TonePortParts::destroyPart(Vc2TonePiece piece) { log(kDestroy, tonePartCode(piece, 0)); }
const Vc2ToneHost& TonePortParts::hostOf(int tone) { return (*s_.parts)[(size_t)tone]; }

void TonePortParts::followLfo(int lfo, int tone, const uint32_t* block) {
    ++s_.follows;
    Vc2ToneHost::followLfo(lfo, tone, block);
}

uint32_t TonePortParts::freeLfo(uint32_t id, int lfo) {
    ++s_.takes;
    return Vc2ToneHost::freeLfo(id, lfo);
}

void TonePortParts::updateOsc(int osc, int32_t n) { log(kUpdateOsc, oscCode(osc), { (uint32_t)n }); }
void TonePortParts::updateCosm(int cosm, float x) { log(kUpdateCosm, cosmCode(cosm), { bits(x) }); }
void TonePortParts::updateTva() { log(kUpdateTva, kTvaCode); }

void TonePortParts::ended() {
    log(kEnded, kToneNone, { s_.pool ? (uint32_t)s_.pool->tones[(size_t)tone()].head.state : 0u });
}

void TonePortParts::renderOsc(int osc, float* buffer) { log(kRenderOsc, oscCode(osc), { bufferOf(buffer) }); }

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

uint32_t TonePortParts::installCosm(int cosm, uint32_t flag) {
    log(kInstallCosm, cosmCode(cosm), { flag });
    return s_.draws.installCosm[(size_t)cosm];
}

void TonePortParts::bindOsc(int osc, const Vc2ToneBinding<4>& b) {
    auto& binds = words.osc[(size_t)osc].binds;
    std::ranges::transform(b.envelopes, binds.begin(), [&](int32_t e) { return toneEnvelopeAt(self(), e); });
    std::ranges::transform(b.lfos, binds.begin() + 4, [&](int32_t l) { return toneLfoAt(self(), l); });
    bindFlags(b.envelopes, b.lfos);
}

void TonePortParts::bindCosm(int cosm, const Vc2ToneBinding<2>& b) {
    const uint32_t numbers = byteOf(b.envelopes[0]) | byteOf(b.envelopes[1]) << 8 | byteOf(b.lfos[0]) << 16
                             | byteOf(b.lfos[1]) << 24;
    log(kBindCosm, cosmCode(cosm), { numbers });
    bindFlags(b.envelopes, b.lfos);
}

void TonePortParts::bindTva(int envelope, int lfoA, int lfoB, int32_t serial) {
    words.tvaBinds = { toneEnvelopeAt(self(), envelope), toneLfoAt(self(), lfoA), toneLfoAt(self(), lfoB),
                       (uint32_t)serial };
    const int32_t envelopes[] = { envelope }, lfos[] = { lfoA, lfoB };
    bindFlags(envelopes, lfos);
}

void TonePortParts::noteOnOsc(int osc, const uint32_t* block, bool legato) {
    log(kNoteOnOsc, oscCode(osc), { blockOf(block), legato ? 1u : 0u });
}

void TonePortParts::takePlayer(int osc, int tone) {
    log(kTakePlayer, oscCode(osc), { (uint32_t)tone, (uint32_t)osc });
}

void TonePortParts::setOsc(int osc, const uint32_t* block) { log(kSetOsc, oscCode(osc), { blockOf(block) }); }

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

// v273: the remaining methods' parts, each logged as rom.4's hook reads it
void TonePortParts::settingsOsc(int osc, const uint32_t* block) { log(kSettingsOsc, oscCode(osc), { blockOf(block) }); }

void TonePortParts::newBlockCosm(int cosm, const uint32_t* block) {
    log(kNewBlockCosm, cosmCode(cosm), { blockOf(block) });
}

void TonePortParts::newBlockTva(const uint32_t* block) { log(kNewBlockTva, kTvaCode, { blockOf(block) }); }
void TonePortParts::legatoOsc(int osc, const uint32_t* ramp) { log(kLegatoOsc, oscCode(osc), { blockOf(ramp) }); }
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
