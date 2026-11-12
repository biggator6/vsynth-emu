// XtSynth's tone update: the Matrix's sums a conversion pass reads, and the blocks and settings an update gives a sounding
// voice.  Moved from XtSynth.cpp as they stood (v193), with an OSC's envelopes made one function (oscEnvs).
#include "XtSynth.h"
#include <algorithm>
#include <cstring>

// The note's object as a conversion pass reads it (the getters 0x894e7010…, 0x894e9fe0): the Matrix's sums, and
// the D-Beam's controllers, each a CC number or −1 (0x894e9db0: OFF and 32).  A value only moves with an
// update, so the sums are kept on the voice between them.
XtMatrixInputs XtSynth::matrixInputs(const Voice& v) const {
    const Controls& Z = zc(v);
    XtMatrixInputs in;
    in.bender = bender; in.pressure = pressure; in.cc = cc; in.poly = polyPress;
    in.velocity = v.mVel; in.key = v.mKey; in.polyKey = v.mRaw; in.pad = pad.radius();
    for (int i = 0; i < 6; ++i) in.assign[i] = std::clamp(Z.assign[4 + i], 0, 95);
    return in;
}

// A key-off converts only the release times (0x894eba30, 0x894df660), of all 13 envelopes whether their OSC or COSM is on or not,
// bar the TVA Switch's and a note value's.  Each reads its getter, and a getter forms its own group of sums, the whole group and
// no other, on what the sources hold now.  So a source that has moved with no update yet, a pedal let go or the pad, reaches
// these ahead of the rest, which what reads the sums at every tick must not see.  The next update forms them all.
void XtSynth::matrixRelease(Voice& v) const {
    const Controls& Z = zc(v);
    const XtMatrixInputs in = matrixInputs(v);
    int16_t next[kXtMatrixSlots];
    xtMatrixSums(Z.matrix, in, next);
    const auto form = [&](int first, int n) { std::memcpy(v.mx + first, next + first, (size_t)n * sizeof next[0]); };
    const auto oscAny = [](const OscControls& o) { return o.pRelease < 128 || o.tRelease < 128 || o.fRelease < 128 || o.oRelease < 128; };
    if (oscAny(Z)) form(xtmx::OscEnv, 12);
    if (oscAny(Z.osc2)) form(xtmx::Osc2Env, 12);
    for (int k = 0; k < 2; ++k)
        if (Z.cosm[k].envA.release < 128 || Z.cosm[k].envB.release < 128) form(k ? xtmx::Cosm2 : xtmx::Cosm, 11);
    if (Z.tvaOn && Z.release < 128) form(xtmx::TvaEnv, 11);
    // 0x894def60 reads each release's term too.  The voice holds them already, as every term that moves asks an update of
    // every voice at once (msmHook) and an edit is the knobs' update, so no check can see this read: it is the firmware's step
    msmTerms(v, true);
}

bool XtSynth::matrixUpdate(Voice& v) const {
    const Controls& Z = zc(v);
    const XtMatrixInputs in = matrixInputs(v);
    int16_t next[kXtMatrixSlots];
    xtMatrixSums(Z.matrix, in, next);
    int beam[4];
    for (int i = 0; i < 4; ++i) {
        const int n = Z.assign[i];
        beam[i] = n <= 0 || n == 32 || n >= 96 ? 0 : cc[n];
    }
    const bool moved = std::memcmp(next, v.mx, sizeof next) != 0 || std::memcmp(beam, v.beam, sizeof beam) != 0;
    std::memcpy(v.mx, next, sizeof next);
    std::memcpy(v.beam, beam, sizeof beam);
    // MFX-PRM1…3 to the effects: each sum with its term, of any zone and part (0x894e7aa0)
    for (int i = 0; i < 3; ++i) {
        const int prm = next[xtmx::MfxPrm + i] + msm.termAll(xtmsm::MfxPrm + i);
        fxPrm[i].store((int16_t)prm, std::memory_order_relaxed);
    }
    const bool termsMoved = msmTerms(v, false);
    return moved || termsMoved;
}

// A note-on's conversion pass: the sums and terms, and the COSMs' input words held until the note has started (tgNoteOn)
bool XtSynth::matrixNoteOn(Voice& v) {
    const bool moved = matrixUpdate(v);
    xtCosmWordsHold(v);
    return moved;
}

void XtSynth::newBlocks(Voice& v) {
    const Controls& Z = zc(v);
    const bool moved = matrixUpdate(v);
    if (Z.on) v.blockTempo = tempoNow();
    oscBeat(v, Z);
    oscBeat(v.osc2, Z.osc2);
    v.mod.setBlock(modBlk[v.zone]);
    tvaNewBlock(v);
    for (int k = 0; k < 2; ++k)
        if (v.cs[k].on) cosmNewBlock(v, k);
    if (moved) updateAll(v);
    lfoUpdates(v);
    if (!moved) return;
    if (v.on) oscFollows(v, v, Z, true, true);       // PORTA-TIME's or a coarse tune's term may have moved
    if (v.osc2.on) oscFollows(v, v.osc2, Z.osc2, true, true);
}

// All a tone's LFOs take the blocks at any update, those whose OSC or COSM is off too.  For an LFO
// on a note value that is more than its own knobs: a key-unsynced note starts with the clock step
// of the tone it follows, and this sets its own.
void XtSynth::lfoUpdates(Voice& v) {
    const Controls& Z = zc(v);
    v.lfo.update(lfoParams(v, v, Z));
    v.osc2.lfo.update(lfoParams(v, v.osc2, Z.osc2));
    for (int k = 0; k < 2; ++k) v.cs[k].lfo.update(clfoParams(v, k));
    v.tlfo.update(tlfoParams(v));
}

// An OSC's four envelopes, pitch, time, formant and amp, at note-on or as an update gives them
void XtSynth::oscEnvs(Voice& v, OscVoice& s, const OscControls& o, bool noteOn) {
    const XtEnvParams p[4] = { oscEnvParams(v, s, 0, o.pAttack, o.pDecay, o.pSustain, o.pRelease),
                               oscEnvParams(v, s, 1, o.tAttack, o.tDecay, o.tSustain, o.tRelease),
                               oscEnvParams(v, s, 2, o.fAttack, o.fDecay, o.fSustain, o.fRelease),
                               ampEnvParams(v, o) };
    XtEnvelope* const env[4] = { &s.penv, &s.tenv, &s.fenv, &s.oenv };
    for (int i = 0; i < 4; ++i) {
        if (noteOn) env[i]->noteOn(p[i]);
        else env[i]->update(p[i]);
    }
}

// Every envelope's settings again, as any tone update gives them (0x894dba00), when a sum has moved them.
void XtSynth::updateAll(Voice& v) {
    const Controls& Z = zc(v);
    v.env.update(envParams(v));
    if (v.on) oscEnvs(v, v, Z, false);
    if (v.osc2.on) oscEnvs(v, v.osc2, Z.osc2, false);
    for (int k = 0; k < 2; ++k) {
        Voice::CosmVoice& c = v.cs[k];
        if (!c.on) continue;
        c.envA.update(cosmEnvParams(v, k, Z.cosm[k].envA));
        c.envB.update(cosmEnvParams(v, k, Z.cosm[k].envB));
    }
}
