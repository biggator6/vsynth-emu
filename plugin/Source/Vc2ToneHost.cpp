// A VC-2 tone's parts' host: see Vc2ToneHost.h (v316).
#include "Vc2ToneHost.h"
#include <cstring>

namespace {
using Piece = Vc2TonePiece;
using Mods = Vc2ToneModulators;
constexpr uint32_t kTvaLevelAt = 0x40;   // [FW] 0x893b4c00: the TVA's word the level test reads, its block's +0x40
constexpr uint32_t kCosmDestroyFlags = 2;   // [FW] 0x893b2fc0: the tone's COSMs destroyed in place, not deleted (bit 0)

bool isCosmPiece(Vc2TonePiece piece) { return piece == Piece::cosm1 || piece == Piece::cosm2; }
size_t cosmOfPiece(Vc2TonePiece piece) { return (size_t)((int)piece - (int)Piece::cosm1); }
int oscOfPiece(Vc2TonePiece piece) { return (int)piece - (int)Piece::osc1; }

Vc2CosmBlockWords blockAt(const uint32_t* block) {   // a converter's block in the note's input
    Vc2CosmBlockWords b;
    std::memcpy(&b, block, sizeof b);
    return b;
}
} // namespace

Vc2ToneHost::Vc2ToneHost(int tone, const Vc2ToneCommon& common)
    : sounds { { Vc2CosmDsp(common.pools), Vc2CosmDsp(common.pools) } }, tone_(tone), c_(common),
      cosmFrame_(common.pools, *this), links_ { { Vc2ToneOscLink(*this, 0), Vc2ToneOscLink(*this, 1) } } {}

// [FW] 0x893b2db0's parts in its order (Vc2TonePiece): the OSC voices' constructor 0x893ae070, the COSMs', the TVA's,
// the mix's, the envelopes' and the LFOs' arrays
void Vc2ToneHost::build(Vc2TonePiece piece) {
    if (piece == Piece::envelopes) mods.buildEnvelopes();
    else if (piece == Piece::lfos) mods.buildLfos(c_.chaos);
    else if (piece == Piece::mix) mix.construct(tail());
    else if (piece == Piece::tva) tva.construct();
    else if (isCosmPiece(piece)) vc2CosmConstruct(cosms[cosmOfPiece(piece)]);
    else voice(oscOfPiece(piece)).construct();
}

// [FW] 0x893b2fc0 takes down the LFOs and the two COSMs alone (Vc2Tone.cpp's kDestroyOrder), all the host's from v318
void Vc2ToneHost::destroy(Vc2TonePiece piece) {
    if (piece == Piece::lfos) mods.destroyLfos();
    else vc2CosmDestroy(cosms[cosmOfPiece(piece)], c_.pools, kCosmDestroyFlags);
}

// [FW] the words the tone reads: an LFO's +0x7c (the free object it follows) and +0x60 (a note value's, listed on the
// clock), the envelopes' +0x68 and the LFOs' +0x80 (ticked when set)
bool Vc2ToneHost::lfoFollows(int lfo) { return mods.lfos[(size_t)lfo].w.follow != 0; }
uint32_t Vc2ToneHost::lfoFollow(int lfo) { return mods.lfos[(size_t)lfo].w.follow; }
bool Vc2ToneHost::lfoListed(int lfo) { return mods.lfos[(size_t)lfo].w.sync != 0; }
bool Vc2ToneHost::envelopeOn(int envelope) { return mods.envelopes[(size_t)envelope].w.on != 0; }
bool Vc2ToneHost::lfoOn(int lfo) { return mods.lfos[(size_t)lfo].w.on != 0; }

void Vc2ToneHost::unfollow(int lfo) { c_.frees.release(mods.lfos[(size_t)lfo].w.follow); }

void Vc2ToneHost::unlist(bool fromOne) {
    if (fromOne) c_.lists.unlistNew(self(), c_.pool);
    else c_.lists.unlistSounding(self(), c_.pool);
}

void Vc2ToneHost::tickEnvelope(int envelope, int32_t n) { mods.envelopes[(size_t)envelope].tick(n); }
void Vc2ToneHost::tickLfo(int lfo, int32_t n) { mods.lfos[(size_t)lfo].tick(n); }
void Vc2ToneHost::clockLfo(int lfo, float x) { mods.lfos[(size_t)lfo].clock(x); }
void Vc2ToneHost::renderMix(const float* a, const float* b, float* c) { mix.render(a, b, c, tail(), c_.rings); }
void Vc2ToneHost::installMix(bool kind3) { mix.install(kind3, tail()); }

void Vc2ToneHost::noteOnEnvelope(int envelope, const uint32_t* block) {
    mods.envelopes[(size_t)envelope].noteOn(block);
}

void Vc2ToneHost::noteOnLfo(int lfo, const uint32_t* block, float x) { mods.lfos[(size_t)lfo].noteOn(block, x); }

void Vc2ToneHost::followLfo(int lfo, int tone, const uint32_t* block) {
    mods.lfos[(size_t)lfo].follow(hostOf(tone).mods.lfos[(size_t)lfo], block, c_.frees);
}

uint32_t Vc2ToneHost::freeLfo(uint32_t id, int lfo) { return c_.frees.take(id, lfo); }
void Vc2ToneHost::attachFree(int lfo, uint32_t free) { mods.lfos[(size_t)lfo].attach(free, c_.frees); }
float Vc2ToneHost::freeValue(uint32_t free) { return c_.frees.object(free).phase; }

void Vc2ToneHost::startFree(int lfo, const uint32_t* block, float, float x) {
    mods.lfos[(size_t)lfo].startFree(block, x, c_.frees);
}

void Vc2ToneHost::relist(uint32_t follow, uint32_t id, int lfo) { c_.frees.addClock(follow, id, lfo); }
void Vc2ToneHost::list(uint32_t id, int lfo) { c_.frees.removeClock(id, lfo); }

// The free object's +0 (the LFO's number) and +4 (the id), as the note-on leaves them
void Vc2ToneHost::markFree(uint32_t free, uint32_t id, int lfo) {
    Vc2FreeLfoWords& o = c_.frees.object(free);
    o.lfo = lfo;
    o.id = id;
}

void Vc2ToneHost::noteOnMix(const uint32_t* block, uint32_t flag1, uint32_t flag2) {
    mix.noteOn(block, flag1, flag2, tail(), c_.rings);
}

void Vc2ToneHost::enlist() { c_.lists.enlist(self(), c_.pool); }
void Vc2ToneHost::settingsLfo(uint32_t lfo, const uint32_t* block) { mods.lfos[lfo].settings(block); }

void Vc2ToneHost::settingsEnvelope(uint32_t envelope, const uint32_t* block) {
    mods.envelopes[envelope].settings(block);
}

void Vc2ToneHost::settingsMix(const uint32_t* block) { mix.settings(block, tail()); }
void Vc2ToneHost::releaseLfo(int lfo) { mods.lfos[(size_t)lfo].release(); }
void Vc2ToneHost::releaseEnvelope(int envelope) { mods.envelopes[(size_t)envelope].release(); }

void Vc2ToneHost::bindFlags(std::span<const int32_t> envelopes, std::span<const int32_t> lfos) {
    for (int32_t e : envelopes) mods.envelopes[(size_t)e].w.on = 1;
    for (int32_t l : lfos) mods.lfos[(size_t)l].w.on = 0;
}

// v317: the TVA
bool Vc2ToneHost::tvaOver() { return tva.over(*this); }
bool Vc2ToneHost::tvaStopped() { return tva.stoppedWord() != 0; }
void Vc2ToneHost::updateTva() { tva.update(*this); }
void Vc2ToneHost::renderTva(const float* in, float* out) { tva.render(in, out, *this); }
float Vc2ToneHost::tvaLevel() { return tva.blockF(kTvaLevelAt); }
bool Vc2ToneHost::tvaHeld() { return tva.silent(*this); }

// The tone's note-on binds its TVA to an envelope and two LFOs of its own, by rom.4's addresses, and its serial
void Vc2ToneHost::bindTva(int envelope, int lfoA, int lfoB, int32_t serial) {
    tva.w.envelope = envelopeAt(envelope);
    tva.w.lfo = lfoAt(lfoA);
    tva.w.panLfo = lfoAt(lfoB);
    tva.w.serial = (uint32_t)serial;
    const int32_t envelopes[] = { envelope }, lfos[] = { lfoA, lfoB };
    bindFlags(envelopes, lfos);
}

void Vc2ToneHost::noteOnTva(const uint32_t* block, uint32_t flag) { tva.noteOn(block, flag, *this); }
void Vc2ToneHost::newBlockTva(const uint32_t* block) { tva.newBlock(block); }
void Vc2ToneHost::truncateTvaTold() { tva.truncateTold(); }
void Vc2ToneHost::truncateTva() { tva.truncate(); }
float Vc2ToneHost::tvaFade() { return tva.w.fade; }

Vc2TvaEnvelope Vc2ToneHost::envelope(uint32_t at) { return mods.tvaEnvelope(self(), at); }
float Vc2ToneHost::lfoOut(uint32_t at) { return mods.lfoOut(self(), at); }
float Vc2ToneHost::fadeStep() { return c_.fadeStep; }
void Vc2ToneHost::setEvent(uint32_t event, uint32_t flags) { osEvent(event, flags); }

// v318: the COSMs.  The tone updates COSM 1 alone, through its vtable's +0x0c (0x893aa7d0), with OSC 1's +0x1cc
void Vc2ToneHost::updateCosm(int cosm, float x) {
    cosmFrame_.update(cosms[(size_t)cosm], x);
    sounds[(size_t)cosm].update(cosms[(size_t)cosm], *this);
}

// The routine the install chose, at its +0x148: the sound in place, the three words after the buffer read past it
void Vc2ToneHost::renderCosm(int cosm, float* buffer) {
    sounds[(size_t)cosm].render(cosms[(size_t)cosm], buffer, c_.past);
}

// 0x893aafe0; a sound whose note waits takes it now, with the install's mono or stereo.  The part installs with a note
// alone: rom.4's note-on holds the install pass inline (nothing reaches 0x893b4060), and the part's structures (0…2)
// always start the COSMs, so a sound keeps its note's mode
uint32_t Vc2ToneHost::installCosm(int cosm, uint32_t flag) {
    const auto k = (size_t)cosm;
    const uint32_t given = vc2CosmInstall(cosms[k], flag);
    if (pending_[k]) sounds[k].noteOn(cosms[k], started_[k], *this, flag != 0);
    pending_[k] = false;
    return given;
}

// 0x893aa7b0 with the bound parts' addresses, envelopes A and B, LFOs A and B; then the binding's flags
void Vc2ToneHost::bindCosm(int cosm, const Vc2ToneBinding<2>& b) {
    vc2CosmBind(cosms[(size_t)cosm],
                { envelopeAt(b.envelopes[0]), envelopeAt(b.envelopes[1]), lfoAt(b.lfos[0]), lfoAt(b.lfos[1]) });
    bindFlags(b.envelopes, b.lfos);
}

// [FW] 0x893b3898…0x893b38c0: the struct the tone's note-on builds on its stack, the OSC's +0x1cc and +0xa80 and its
// byte +0x58 at +0xc; its +8 the set-up writes as the flag, and the three bytes after the byte are the stack's, which
// nothing reads
void Vc2ToneHost::setUpCosm(int cosm, const Vc2ToneCosmSetUp& s, uint32_t flag) {
    vc2CosmSetUp(cosms[(size_t)cosm], { s.value, s.other, 0, s.byte }, flag);
}

// 0x893aa0e0 on pool `which`, COSM 1's 0 and COSM 2's 1; the block kept for its sound's note-on at the install
void Vc2ToneHost::startCosm(int cosm, const uint32_t* block, int which) {
    const auto k = (size_t)cosm;
    started_[k] = blockAt(block);
    cosmFrame_.noteOn(cosms[k], started_[k], (uint32_t)which);
    pending_[k] = true;
}

void Vc2ToneHost::newBlockCosm(int cosm, const uint32_t* block) {
    const Vc2CosmBlockWords b = blockAt(block);
    cosmFrame_.newBlock(cosms[(size_t)cosm], b);
    sounds[(size_t)cosm].newBlock(cosms[(size_t)cosm], b, *this);
}

float Vc2ToneHost::cosmCost() { return cosms[0].factor; }   // [FW] COSM 1's +0x14c, the install's float
float Vc2ToneHost::word(uint32_t at) { return mods.wordAt(self(), at); }

uint32_t Vc2ToneHost::envelopeAt(int envelope) const {
    return self() + Mods::kEnvelopesAt + Mods::kEnvelopeBytes * (uint32_t)envelope;
}

uint32_t Vc2ToneHost::lfoAt(int lfo) const { return self() + Mods::kLfosAt + Mods::kLfoBytes * (uint32_t)lfo; }
