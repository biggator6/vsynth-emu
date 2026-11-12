// XtSynth's envelopes', LFOs', TVA's and COSMs' knobs as the converters form them: each envelope's times (0x894deb60 attack,
// 0x894ded60 decay, 0x894df170 sustain, 0x894def60 release), each LFO's rate (0x894e00b0), the patch TVA (0x894ea380,
// 0x894eab40) and a COSM's input words (0x894de3b0).  Moved from XtSynth.cpp (v193), with the Multi Step Modulator's terms
// where those routines take them (HARDWARE_FIRMWARE.md, "The Multi Step Modulator in XtSynth").
#include "XtSynth.h"
#include <algorithm>
#include <iterator>

namespace {
// An LFO's rate or an envelope's time takes the Matrix's sum within 0…127, and a note value, from 128 on, takes none
// (0x894e00b0; 0x894df2ac in the converter, 0x894deb60).  So a sum cannot carry a knob across to a note value.
// The Multi Step Modulator's term goes in with the sum.
int withSum(int knob, int sum) { return knob >= 128 ? knob : std::clamp(knob + sum, 0, 127); }

struct EnvTimes {
    int attack, decay, sustain, release;
};

// An envelope's times and Sustain: each time with its Matrix's sum (sums: the Attack's, Decay's and Release's) and its term
// (terms: by EnvStage; none for the OSCs' envelopes, which the Multi Step Modulator has no destination for), the Sustain
// with its term alone, within 0…127 (0x894df170).
// one-off: the converter's values, each formed from its knob, sum and term; xtEnvAdsr's overload in XtEnvelope.h sets
// plain knobs
void envTimes(XtEnvKnobs& k, const EnvTimes& knob, const int16_t* sums, const int16_t* terms) {
    static constexpr int16_t kNone[4] = {};
    const int16_t* t = terms ? terms : kNone;
    k.attack = withSum(knob.attack, sums[0] + t[xtmsm::Attack]);
    k.decay = withSum(knob.decay, sums[1] + t[xtmsm::Decay]);
    k.sustain = std::clamp(knob.sustain + t[xtmsm::Sustain], 0, 127);
    k.release = withSum(knob.release, sums[2] + t[xtmsm::Release]);
}
}  // namespace

XtEnvParams XtSynth::ampEnvParams(const Voice& v, const OscControls& o) const {
    const Controls& Z = zc(v);
    XtEnvKnobs k;
    const int16_t* m = v.mx + (&o == &Z ? xtmx::OscEnv : xtmx::Osc2Env) + 9;
    k.tempo10 = v.tempo10;
    envTimes(k, { o.oAttack, o.oDecay, o.oSustain, o.oRelease }, m, nullptr);
    k.aSens = o.oASens; k.dSens = o.oDSens; k.rSens = o.oRSens; k.timeKF = o.oTimeKF;
    return xtEnvAdsr(k, v.note, v.vel, v.relVel, 2);
}

XtLfoParams XtSynth::lfoParams(const Voice& v, const OscVoice& s, const OscControls& o) const {
    const int sum = v.mx[&s == &v ? xtmx::OscLfoRate : xtmx::Osc2Lfo];
    const int term = v.msm[(&s == &v ? xtmsm::Osc1 : xtmsm::Osc2) + xtmsm::OscLfoRate];
    const int rate = withSum(o.lfoRate, sum + term);
    return xtLfoConvert(o.lfoWave, rate, o.lfoOffset, o.lfoDelay, o.lfoFadeMode, o.lfoFade, o.lfoKeySync);
}

XtLfoParams XtSynth::clfoParams(const Voice& v, int k) const {
    const Controls& Z = zc(v);
    const Controls::Cosm& c = Z.cosm[k];
    const int sum = v.mx[(k ? xtmx::Cosm2 : xtmx::Cosm) + xtmx::CosmLfoRate];
    const int term = v.msm[(k ? xtmsm::Cosm2 : xtmsm::Cosm1) + xtmsm::CosmLfoRate];
    const int rate = withSum(c.lfoRate, sum + term);
    return xtLfoConvert(c.lfoWave, rate, c.lfoOffset, c.lfoDelay, c.lfoFadeMode, c.lfoFade, c.lfoKeySync);
}

XtLfoParams XtSynth::tlfoParams(const Voice& v) const {
    const Controls& Z = zc(v);
    const int rate = withSum(Z.tvaLfoRate, v.mx[xtmx::TvaLfoRate] + v.msm[xtmsm::Tva + xtmsm::TvaLfoRate]);
    return xtLfoConvert(Z.tvaLfoWave, rate, Z.tvaLfoOffset, Z.tvaLfoDelay, Z.tvaLfoFadeMode, Z.tvaLfoFade,
                        Z.tvaLfoKeySync);
}

// The TVA's knobs, each with its term and the Matrix's sum; the sends' terms are the zone's too
XtTvaBlock XtSynth::tvaBlock(const Voice& v) const {
    const Controls& Z = zc(v);
    XtTvaKnobs k;
    k.on = Z.tvaOn;
    using namespace xtmx;
    const int16_t* t = v.msm + xtmsm::Tva;
    k.level = Z.tvaLevel + t[xtmsm::TvaLevel] + v.mx[TvaLevel];
    k.levelKF = Z.tvaLevelKF;
    k.levelLfo = Z.tvaLevelLfo + t[xtmsm::TvaLfoLevel] + v.mx[TvaLfoLevel];
    k.pan = Z.tvaPan + t[xtmsm::TvaPan] + v.mx[TvaPan];
    k.panKF = Z.tvaPanKF;
    k.panLfo = Z.tvaPanLfo + t[xtmsm::TvaLfoPan] + v.mx[TvaLfoPan];
    k.send = Z.mfxSend + v.msm[xtmsm::MfxSend] + v.mx[MfxSend];
    k.chorusSend = Z.chorusSend + v.msm[xtmsm::ChoSend] + v.mx[ChorusSend];
    k.reverbSend = Z.reverbSend + v.msm[xtmsm::RevSend] + v.mx[ReverbSend];
    k.assign = Z.outputAssign; k.parallel = Z.parallel; k.outGain = Z.outGainTg;
    k.cc7 = cc[7]; k.cc11 = cc[11]; k.cc10 = cc[10]; k.cc91 = cc[91]; k.cc93 = cc[93];
    return xtTvaBlock(k, v.note, v.velGain);
}

// A COSM envelope (ADSR kinds 8..11, curve 2): its times as the other
// envelopes'; its Velocity Curve and Sens scale the COSM's depth instead.
XtEnvParams XtSynth::cosmEnvParams(const Voice& v, int cosm, const Controls::CosmEnv& e) const {
    const Controls& Z = zc(v);
    XtEnvKnobs k;
    const bool first = &e == &Z.cosm[cosm].envA;
    const int16_t* m = v.mx + (cosm ? xtmx::Cosm2 : xtmx::Cosm) + (first ? xtmx::CosmEnv1 : xtmx::CosmEnv2);
    const int16_t* t = v.msm + (cosm ? xtmsm::Cosm2 : xtmsm::Cosm1) + (first ? xtmsm::CosmEnv1 : xtmsm::CosmEnv2);
    k.tempo10 = v.tempo10;
    envTimes(k, { e.attack, e.decay, e.sustain, e.release }, m, t);
    k.aSens = e.aSens; k.dSens = e.dSens; k.rSens = e.rSens; k.timeKF = e.timeKF;
    return xtEnvAdsr(k, v.note, v.vel, v.relVel, 2);
}

XtEnvParams XtSynth::oscEnvParams(const Voice& v, const OscVoice& s, int which, int a, int d, int su, int r) const {
    const Controls& Z = zc(v);
    XtEnvKnobs k;
    const int16_t* m = v.mx + (&s == &v ? xtmx::OscEnv : xtmx::Osc2Env) + 3 * which;
    k.tempo10 = v.tempo10;
    envTimes(k, { a, d, su, r }, m, nullptr);
    const OscControls& o = &s == &v ? static_cast<const OscControls&>(Z) : Z.osc2;
    const OscControls::EnvVelocity& e = which == 0 ? o.pVel : which == 1 ? o.tVel : o.fVel;
    k.aSens = e.aSens; k.dSens = e.dSens; k.rSens = e.rSens; k.timeKF = e.timeKF;
    return xtEnvAdsr(k, v.note, v.vel, v.relVel, 0);
}

XtEnvParams XtSynth::envParams(const Voice& v) const {
    const Controls& Z = zc(v);
    XtEnvKnobs k;
    k.tempo10 = v.tempo10;
    envTimes(k, { Z.attack, Z.decay, Z.sustain, Z.release }, v.mx + xtmx::TvaEnv, v.msm + xtmsm::Tva + xtmsm::TvaEnv);
    k.aSens = Z.aSens; k.dSens = Z.dSens; k.rSens = Z.rSens; k.timeKF = Z.timeKF;
    XtEnvParams p = xtEnvAdsr(k, v.note, v.vel, v.relVel);
    if (!Z.tvaOn) {                                  // the TVA Switch off: no times, level 1
        for (float& t : p.time) t = 0.0f;
        p.level[3] = 1.0f;
    }
    return p;
}

namespace {
// The four input words: each one's slot in the COSM's group of the Matrix's sums, and its destination
struct CosmWord {
    int slot, dest;
};
constexpr CosmWord kCosmWords[4] = { { xtmx::CosmPrm1, xtmsm::CosmPrm1 }, { xtmx::CosmLfoPrm1, xtmsm::CosmLfoPrm1 },
                                     { xtmx::CosmPrm2, xtmsm::CosmPrm2 }, { xtmx::CosmLfoPrm2, xtmsm::CosmLfoPrm2 } };

int16_t* cosmSums(XtVoice& v, int k) {
    return v.mx + (k ? xtmx::Cosm2 : xtmx::Cosm);
}
}  // namespace

// Each stored as a word, the sum with its term (0x894de452…0x894de534)
void xtCosmWordsHold(XtVoice& v) {
    if (v.cosmHeld) return;
    v.cosmHeld = true;
    for (int k = 0; k < 2; ++k) {
        int16_t* m = cosmSums(v, k);
        const int16_t* t = v.msm + (k ? xtmsm::Cosm2 : xtmsm::Cosm1);
        for (int i = 0; i < 4; ++i) {
            v.cosmSums[k][i] = m[kCosmWords[i].slot];
            m[kCosmWords[i].slot] = (int16_t)(v.cosmSums[k][i] + t[kCosmWords[i].dest]);
        }
    }
}

void xtCosmWordsRelease(XtVoice& v) {
    if (!v.cosmHeld) return;
    v.cosmHeld = false;
    for (int k = 0; k < 2; ++k)
        for (int i = 0; i < 4; ++i) cosmSums(v, k)[kCosmWords[i].slot] = v.cosmSums[k][i];
}

void XtSynth::cosmNewBlock(Voice& v, int k) {
    xtCosmWordsHold(v);
    cosmSetBlock(v, k);
    xtCosmWordsRelease(v);
}
