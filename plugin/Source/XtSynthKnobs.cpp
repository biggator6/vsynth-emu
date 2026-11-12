// XtSynth's OSC knobs as the converters form them: the note's tuning (0x894e48a0, 0x894e4c30, 0x894e4e90), the analog
// converter (0x894e1920) and the VariPhrase one (0x894e2f20, LITE's too), the portamento (0x894e6510), and the players'
// targets from them.  Moved from XtSynth.cpp (v193).  Each knob takes the Multi Step Modulator's term where its converter
// does: on the knob, ahead of the Matrix's sum and the limit the sum already has (HARDWARE_FIRMWARE.md, "The Multi Step
// Modulator in XtSynth").
#include "XtSynth.h"
#include <algorithm>
#include <cmath>
#include <iterator>
#include "XtFloat.h"

namespace {
// An OSC's first destination: OSC 1's from 3, OSC 2's from 17
int oscFirst(const XtVoice& v, const XtOscVoice& s) {
    return &s == &v ? xtmsm::Osc1 : xtmsm::Osc2;
}

int oscTerm(const XtVoice& v, const XtOscVoice& s, int k) {
    return v.msm[oscFirst(v, s) + k];
}

// The patch's tuning: Pitch Shift as semitones and the cents left, ±50, on Patch Coarse and Fine; and the Scale Tune
XtTuning patchTuning(const XtControls& Z) {
    const long shift = std::lround((double)Z.pitchSemis * 100.0);
    const int semis = (int)std::lround((double)shift / 100.0);
    XtTuning t;
    t.coarse = Z.patchCoarse + semis;
    t.fine = Z.patchFine + (int)(shift - semis * 100L);
    t.scaleOn = Z.scaleTune;
    std::copy(std::begin(Z.scale), std::end(Z.scale), t.scale);
    return t;
}

// The note's coarse word (+0x68) with OSC1+2-PIT-CRS's term and the OSC's, `osc` the OSC's first destination: the tuning
// routines add both, each through the coarse curve, after the patch's and the OSC's Coarse
int coarseWord(const XtVoice& v, int osc, const XtTuning& t, int oscCoarse) {
    return (int16_t)(xtNoteCoarse(t, oscCoarse) + v.msm[xtmsm::PitCrs] + v.msm[osc + xtmsm::OscCrs]);
}
}  // namespace

// The player's targets (0x894f3ba0 pitch, 0x894f3d40 formant, 0x894f3c90
// walk, voice +0x18 Energy), from the converters' values (XtOsc): the pitch
// ratio from the note's cents with the pitch envelope and the LFO at their
// depths, within ±32; formant and walk with their envelopes and the LFO,
// the formant ratio within [0.5, 2], the walk within ±5 (negative: the
// players run backward, a note starting at the wave's end).
// The voice's cents at a key (the converter's +8; at the previous key its
// +0x68): the note's Coarse and Fine (+0x68, +0x70) as the per-note setup
// forms them, from the patch's tuning (Pitch Shift as semitones and the cents
// left, ±50, added to Patch Coarse and Fine), the OSC's, the Scale Tune at
// the note's key, the bend and the note's random offset; formed again at
// every update, as an update does on the XT (0x894e4e90, the random kept).
// Robot Voice takes the pitch against A4 (0x894e4030), not against the
// wave's Original Key, and so does the analog oscillator (0x894e1920).
// The fine takes ten times OSC1+2-PIT-FINE's term and the OSC's.
void XtSynth::noteTuning(const Voice& v, const OscVoice& s, const OscControls& o, int& coarse, int& fine) const {
    const Controls& Z = zc(v);
    const XtTuning t = patchTuning(Z);
    const int osc = oscFirst(v, s);
    coarse = coarseWord(v, osc, t, o.oscCoarse);
    const int bend = xtPitchBend(bender, Z.bendUp, Z.bendDown);
    const int fineTerms = v.msm[xtmsm::PitFine] + v.msm[osc + xtmsm::OscFine];
    fine = xtNoteFine(t, v.note, o.oscFine, bend, s.rnd) + 10 * fineTerms;
}

float XtSynth::noteCents(const Voice& v, const OscVoice& s, const OscControls& o, int key) const {
    int coarse, fine;
    noteTuning(v, s, o, coarse, fine);
    return xtPitchCents(key, coarse, 10 * o.pitchKF, fine, s.analog ? 69 : o.rootKey, s.robotKnob && o.vari);
}

// The analog converter's block at the voice's note, formed again at every
// update as the knobs are (the level follows the note's pitch, the bend and
// the random offset included).  The time and formant envelopes' velocity
// curves are off: factors 1.  Each knob with its term (0x894e1f78…0x894e2050): FORMA/FAT's on both Fat and Detune.
XtAnalogBlock XtSynth::analogBlock(const Voice& v, const OscVoice& s, const OscControls& o) const {
    int coarse, fine;
    noteTuning(v, s, o, coarse, fine);
    using namespace xtmx;
    const int dO = &s == &v ? 0 : Osc2, dL = &s == &v ? OscLfoRate : Osc2Lfo;
    XtAnalogParams a = o.ana;
    a.pw += oscTerm(v, s, xtmsm::OscTime);
    a.pwLfo += oscTerm(v, s, xtmsm::OscLfoTime) + v.mx[dL + 2];
    a.pwEnv += oscTerm(v, s, xtmsm::OscTenv);
    a.fat += oscTerm(v, s, xtmsm::OscFormant);
    a.detune += oscTerm(v, s, xtmsm::OscFormant);
    a.fatLfo += oscTerm(v, s, xtmsm::OscLfoFormant) + v.mx[dL + 3];
    a.fatEnv += oscTerm(v, s, xtmsm::OscFenv);
    const XtAnalogMod m{ v.mx[OscTime + dO], v.mx[OscFormant + dO], v.beam[0], v.beam[1] };
    const int pitch10 = xtPitch10(v.note, coarse, 10 * o.pitchKF, fine);
    const float fatVel = xtCosmVelocity(v.vel, o.fVel.sens, o.fVel.curve);
    const float pwVel = xtCosmVelocity(v.vel, o.tVel.sens, o.tVel.curve);
    return xtAnalogBlock(a, v.note, pitch10, o.waveGain, fatVel, pwVel, &s == &v.osc2, m);
}

// The Pitch knob's ratio (+0xc): the knob with its term, the Matrix's sum, and 2·b183 − b182 from the D-Beam's
float XtSynth::pitchKnob(const Voice& v, const OscVoice& s, const OscControls& o) const {
    const int knob = o.oscPitch + oscTerm(v, s, xtmsm::OscPitch);
    const int sum = v.mx[xtmx::OscPitch + (&s == &v ? 0 : xtmx::Osc2)];
    return xtPitchRatio(knob, sum, 2 * v.beam[3] - v.beam[2]);
}

// one-off: the voice update in mixBlock takes its four values by reference, and mixBlock (154 statements) can't change
// until it is split
void XtSynth::targets(const Voice& v, const OscVoice& s, const OscControls& o, float& A, float& F, float& walk, float& g,
                      bool noteOn) const {
    using namespace xtmx;
    const int dO = &s == &v ? 0 : Osc2, dL = &s == &v ? OscLfoRate : Osc2Lfo;
    // The wave's own tuning: none on an analog voice, and with Robot Voice none on the pitch (0x894f21c0)
    const XtWaveTune tune = !s.analog && s.wave ? s.wave->tune : XtWaveTune{};
    const bool tuned = !s.robot;
    // Each envelope's depth takes its velocity factor (1 with no curve)
    const auto withVel = [&v](float depth, const OscControls::EnvVelocity& e) {
        return xtf::mul(depth, xtCosmVelocity(v.vel, e.sens, e.curve));
    };
    const int pDepth = std::clamp(o.pDepth + oscTerm(v, s, xtmsm::OscPenv), -63, 63);
    const int pLfo = std::clamp(o.lfoPitchDepth + oscTerm(v, s, xtmsm::OscLfoPitch) + v.mx[dL + 1], -63, 63);
    const float pEnv = withVel(xtPitchDepthCents((float)pDepth), o.pVel);
    const float offset = tuned ? tune.offset : 0.0f, detune = tuned ? tune.detune : 0.0f;
    A = xtPitchTarget(noteCents(v, s, o, v.note), s.penv.level(), pEnv, s.lfo.out(), xtPitchDepthCents((float)pLfo),
                      pitchKnob(v, s, o), s.porta.value(), offset, 1.0f, detune);
    const int formant = o.oscFormant + oscTerm(v, s, xtmsm::OscFormant);
    const float fEnv = withVel(xtOscFormantEnvDepth(o.fDepth + oscTerm(v, s, xtmsm::OscFenv)), o.fVel);
    const int fLfo = o.lfoFormantDepth + oscTerm(v, s, xtmsm::OscLfoFormant) + v.mx[dL + 3];
    // kind 1's note-on and Vari off: 0x894f3b30, no factor
    const float fFactor = s.lite && (s.linear || noteOn) ? 1.0f : tune.factor;
    F = xtOscFormantRatio(xtOscFormant(formant, o.oscFormantKF, v.note, v.mx[OscFormant + dO]), s.fenv.level(), fEnv,
                          s.lfo.out(), xtOscFormantLfoDepth(fLfo), fFactor);
    const int time = o.oscTime + oscTerm(v, s, xtmsm::OscTime);
    const float tEnv = withVel(xtOscTimeEnvDepth(o.tDepth + oscTerm(v, s, xtmsm::OscTenv)), o.tVel);
    const int tLfo = o.lfoTimeDepth + oscTerm(v, s, xtmsm::OscLfoTime) + v.mx[dL + 2];
    // kind 1: note-on and Vari off 0x894f3ac0, no factor; on 0x894f3c90, whose test reads +0xb4, the Loop word here.  The
    // others 0x894f3cd0, past Tempo Sync's test
    const float liteFactor = s.linear || noteOn ? 1.0f : tune.factor;
    const float wFactor = s.lite ? liteFactor : s.variBit || s.syncBit ? tune.factor : 1.0f;
    const int beams = v.beam[1] - 3 * v.beam[0];
    const float oscTime = xtOscTime(time, o.oscTimeOffset, o.oscTimeKF, v.note, v.mx[OscTime + dO], beams);
    walk = xtOscWalk(oscTime, s.tenv.level(), tEnv, s.lfo.out(), xtOscTimeLfoDepth(tLfo), wFactor);
    g = xtOscEnergy(o.energy);
}

// The player's gain target (0x894f3dc0): the OSC gain times the OSC level
// (velocity included) times the OSC amp envelope, with the LFO's tremolo.
// The analog oscillator's gain carries its waveform's level instead of the
// wave's (the converter's +0x5c).
float XtSynth::gainTarget(const Voice& v, const OscVoice& s, const OscControls& o, float env) const {
    using namespace xtmx;
    const int dO = &s == &v ? 0 : Osc2, dL = &s == &v ? OscLfoRate : Osc2Lfo;
    const int knob = o.oscLevel + oscTerm(v, s, xtmsm::OscLevel) + v.mx[OscLevel + dO];
    const float level = xtOscLevel(knob, o.oscLevelKF, v.note, xtVelGain(v.vel, o.oscVelSens, o.oscVelCurve));
    const float gain = s.analog ? analogBlock(v, s, o).gain : xtOscGain(o.waveGain);
    const int lfo = o.oscLevelLfoDepth + oscTerm(v, s, xtmsm::OscLfoLevel) + v.mx[dL + 4];
    return xtOscGainTarget(gain, level, env, s.lfo.out(), xtOscLevelLfoDepth(lfo));
}

// The key-follow key (the OSC's +4), from the coarse word with its terms
int XtSynth::keyFollowKey(const Voice& v, const OscControls& o) const {
    const Controls& Z = zc(v);
    const int osc = &o == &Z.osc2 ? xtmsm::Osc2 : xtmsm::Osc1;
    const int coarse = coarseWord(v, osc, patchTuning(Z), o.oscCoarse);
    return xtKeyFollowKey(v.note, coarse, 10 * o.pitchKF);
}

// The portamento's settings (0x894e6510): its Time with PORTA-TIME's term, of any zone (0x894d9750), which the converter
// takes as a byte and limits to 127 from above only, so a sum below 0 is 127 too
XtPortaSettings XtSynth::portaSettings(const Voice& v) const {
    const Controls& Z = zc(v);
    const int time = std::min((Z.portaTime + v.msm[xtmsm::PortaTime]) & 0xff, 127);
    return xtPortaSettings(Z.portamento, Z.portaType, time, Z.portaVeloSens, v.vel);
}

// What a tone update gives an OSC's glide (0x894ebd34 → 0x894f48b0): the portamento's settings when they may have moved,
// and when the key-follow key may have, from the voice's source key again if it did (0x894f2600)
void XtSynth::oscFollows(Voice& v, OscVoice& s, const OscControls& o, bool porta, bool key) {
    if (porta) s.porta.update(portaSettings(v));
    if (!key) return;
    const int k = keyFollowKey(v, o);
    if (k == s.kfKey) return;
    s.kfKey = k;
    s.porta.restart(noteCents(v, s, o, v.srcKey), noteCents(v, s, o, v.note));
}
