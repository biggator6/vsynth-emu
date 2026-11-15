// A voice's start: Playback Mode LEGATO's take of an earlier player, and startVoice, the note's whole start (moved from
// XtSynth.cpp as they stood, v203).
#include "XtSynth.h"
#include "XtSynthConstants.h"
#include "XtFloat.h"

#include <algorithm>
#include <iterator>

using namespace xtsynth;

// Playback Mode LEGATO (0x894f2d00): the voice takes the earlier note's whole player (its +0xd4…+0x87c), its own pitch, formant,
// walk, Energy and gain target (+0x144…+0x154) put back, and the gain from 0 (+0x158).  Not if that player is done (its +4).
void XtSynth::legatoTake(OscVoice& s, const OscVoice& from) {
    s.staleEnd = from.staleEnd;                     // the copy's +4
    if (s.analog) s.ana = from.ana;
    else if (s.linear) s.lin = from.lin;
    else if (s.lite) s.xfg = from.xfg;
    else if (s.wave->type == 1) s.solo = from.solo;
    else s.mb.takeState(from.mb);
    if (s.analog) return;
    if (s.linear) s.lin.setGainXt(0.0f, true);
    else if (s.lite) s.xfg.setGainXt(0.0f, true);
    else if (s.wave->type == 1) s.solo.resetGain(0.0f);
    else s.mb.setGainXt(0.0f, true);
    if (s.lite && !s.linear) { s.xfg.takeRates(s.curA, s.curF, s.curWalk); s.xfg.setGainXt(s.curGain); }   // its filter's rate is the player's, kept
    else applyRates(s);
}

// one-off: the firmware's whole note start in its order (151 statements); its split by job needs member functions that
// XtSynth.h, at its line limit, has no room to declare, and waits for a version of its own (build.md, v203's entry)
void XtSynth::startVoice(int raw, int note, int vel, int zone) {
    const Controls& Z = zc(zone);                      // the zone's, all through the note's start
    xtModBlock(Z.mod, modBlk[zone]);                 // the tables may have loaded since the last knob moved
    // portamento's source key (0x894e4940): the previous key plus the Octave
    // Shift; with LEGATO the note's own (no glide) unless a key was held
    // then (POLY: or Hold is down)
    const bool held = Z.mono ? keys.prevMonoHeld : keys.prevPolyHeld || hold >= 64;
    const int srcKey = Z.portaMode == 1 && !held ? note : shiftKey(Z.mono ? keys.prevMono : keys.prevPoly);
    Voice* v = nullptr;
    for (Voice& w : voice) if (!w.active) { v = &w; break; }
    if (!v) { v = &voice[0]; for (Voice& w : voice) if (w.age < v->age) v = &w; }   // steal the oldest
    // the voice's stop (0x895053c0) frees FEEDBACK-OSC's line; only a note-on can
    // tell when it happened
    for (Voice& w : voice) if (!w.active) { w.ana.release(); w.osc2.ana.release(); }
    v->ana.release();
    v->osc2.ana.release();
    // The allocator's earlier note of the part and zone (0x894d4c04), among those not released, oldest first: the first whose key is
    // down, else the last.  The new note takes its chain key (+0x6c4), and with Playback Mode LEGATO and its key down, its player.
    const Voice* prev = nullptr;
    bool prevHeld = false;
    {
        const Voice* order[kVoices];
        int n = 0;
        for (const Voice& w : voice)
            if (&w != v && w.active && !w.releasing && w.zone == zone) order[n++] = &w;
        std::sort(order, order + n, [](const Voice* a, const Voice* b) { return a->age < b->age; });
        for (int i = 0; i < n && !prevHeld; ++i) { prev = order[i]; prevHeld = (order[i]->keyFlags & 1) != 0; }
    }
    v->chainKey = prev ? prev->chainKey : note;
    v->steps = stepCount[zone][0]++;                 // the per-note OSC setup (0x894e4940): the count, then one more
    v->osc2.steps = stepCount[zone][1]++;
    v->active = true;
    v->zone = zone;
    v->releasing = false;
    v->damping = false;
    v->note = note;
    v->raw = raw;
    v->srcKey = srcKey;
    v->age = clock++;
    v->structure = std::clamp(Z.structure, 0, 2);
    v->vel = vel;
    v->relVel = 64;
    v->keyFlags = 7;
    v->mVel = vel; v->mKey = note; v->mRaw = raw;
    matrixNoteOn(*v);
    v->tempo10 = tempo10Now();
    if (Z.on) v->blockTempo = tempoNow();
    v->on = oscPlays(zone, 0);
    v->osc2.on = oscPlays(zone, 1);
    v->analog = Z.oscType == 0;
    v->osc2.analog = Z.osc2.oscType == 0;
    v->extIn = Z.oscType == 2;                       // kind 4 (0x894f2050): the input's player, and no wave
    v->osc2.extIn = Z.osc2.oscType == 2;
    // A PCM OSC sounding: what reads a wave
    const auto pcm = [](const OscVoice& s) { return s.on && !s.analog && !s.extIn; };
    // LEGATO taken up: the PCM converter's tail (0x894e2dac) sets the block's +0x64 with the earlier note's key down.  It also picks a
    // wave set's wave at the chain's key (0x894e1570), which a single wave doesn't have
    const bool legato1 = prevHeld && Z.playbackMode == 1 && pcm(*v), legato2 = prevHeld && Z.osc2.playbackMode == 1 && pcm(v->osc2);
    v->wave = v->extIn ? nullptr : waveOf(zone, 0);
    v->osc2.wave = v->osc2.extIn ? nullptr : waveOf(zone, 1);
    v->robotKnob = Z.robot && pcm(*v) && v->wave->type == 1;   // SOLO only (the converter's SOLO tail)
    v->osc2.robotKnob = Z.osc2.robot && pcm(v->osc2) && v->osc2.wave->type == 1;
    v->robot = v->robotKnob && Z.vari;                                     // the block's flag 0x10 takes Vari Sw on too
    v->osc2.robot = v->osc2.robotKnob && Z.osc2.vari;
    v->lite = pcm(*v) && v->wave->type == 0;
    v->linear = !Z.vari && pcm(*v) && (v->wave->type == 1 || v->lite);
    v->variBit = Z.vari;
    v->osc2.variBit = Z.osc2.vari;
    v->syncBit = Z.tempoSync;
    v->osc2.syncBit = Z.osc2.tempoSync;
    v->osc2.lite = pcm(v->osc2) && v->osc2.wave->type == 0;
    // A LITE voice's root frequency (+0x9a4), which its Vari-on player's period and filter go by
    if (v->lite) v->root = xtCosmRootHz(Z.rootKey, false, v->wave->tune.offset, v->wave->tune.detune);
    if (v->osc2.lite) v->osc2.root = xtCosmRootHz(Z.osc2.rootKey, false, v->osc2.wave->tune.offset, v->osc2.wave->tune.detune);
    v->osc2.linear = !Z.osc2.vari && pcm(v->osc2) && (v->osc2.wave->type == 1 || v->osc2.lite);
    // the OSCs' random pitch (0x894e4940, OSC 1 then 2, each whatever its
    // Switch): the shared generator steps once per note for an OSC with a depth
    v->rnd = pitchRandom.next(xtRandomPitchDepth(Z.randomPitch));
    v->osc2.rnd = pitchRandom.next(xtRandomPitchDepth(Z.osc2.randomPitch));
    v->velGain = xtVelGain(v->vel, Z.ampVelSens, Z.ampVelCurve);
    v->tb = tvaBlock(*v);
    // the tone is constructed for each note (0x894f5860): LFOs 0…7 draw their
    // chaos seeds, and the pitch ramps (0x894f4860) start off, so the note's
    // first pitch has no glide in it
    float seed[8];
    for (float& x : seed) x = drawSeed();
    v->porta = XtPitchRamp{};
    v->osc2.porta = XtPitchRamp{};
    using namespace xtf;
    const XtEnvParams ep = envParams(*v);
    v->env.noteOn(ep);
    // The note-on converters set the player directly (0x894f3a00…)
    // a key-unsynced LFO starts from the newest other tone of the part and zone sounding, if
    // there is one (the tones' lists at 0x89f91c50/54, newest first, by the tone's +0x1c), else
    // from its zone's free-running phase
    const Voice* other = nullptr;
    for (const Voice& w : voice)
        if (&w != v && w.active && w.zone == zone && (!other || w.age > other->age)) other = &w;
    oscLfoNoteOn(*v, *v, Z, freeLfo[zone], other ? &other->lfo : nullptr, seed[1]);
    oscLfoNoteOn(*v, v->osc2, Z.osc2, freeLfo2[zone], other ? &other->osc2.lfo : nullptr, seed[2]);
    if (Z.tvaLfoKeySync) v->tlfo.noteOn(tlfoParams(*v), seed[5], v->blockTempo);
    else if (other) v->tlfo.noteOnFollow(tlfoParams(*v), other->tlfo);
    else v->tlfo.noteOnFree(tlfoParams(*v), seed[5], freeTlfo[zone], v->blockTempo);
    // TVA note-on (0x894f50e0), after the LFOs: the gain starts from the level times the
    // envelope's onset level (full with no attack, else 0) and ramps to the level
    // times its output over the attack time, at most 10 ms (0.1 ms with no attack), each
    // with the LFO on the level and the pan.  The output bus's level starts where
    // the block has it.
    tvaGains(*v, mul(v->env.onsetLevel(), v->tb.level), v->cur);
    tvaGains(*v, mul(v->env.level(), v->tb.level), v->target);
    const float ms = ep.time[0] == 0.0f ? (ep.time[1] == 0.0f ? kNoAttackMs : ep.time[1]) : (10.0f > ep.time[0] ? ep.time[0] : 10.0f);
    const float frames = mul(ms, kFramesPerMs);
    v->step[0] = div(sub(v->target[0], v->cur[0]), frames);
    v->step[1] = div(sub(v->target[1], v->cur[1]), frames);
    std::copy(std::begin(v->tb.bus), std::end(v->tb.bus), v->bus);
    v->elapsed = 0;
    if (v->on) oscEnvs(*v, *v, Z, true);
    if (v->osc2.on) oscEnvs(*v, v->osc2, Z.osc2, true);
    // the COSM sections (0x894edf70, COSM 1 then 2): their
    // envelopes and LFOs start with the others; the filters' states clear and
    // their coefficients start from the envelopes' start levels
    // the voices' stereo flags as their setups leave them (+0x960): a stereo wave's on a PCM OSC, always on an EXT IN voice
    // (kind 4's install leaves it as set), and none on an analog voice (0x89505170 clears it) or an OSC that is off
    auto stereo = [&pcm](const OscVoice& s) { return (pcm(s) && s.wave->channels == 2) || (s.on && s.extIn); };
    for (int k = 0; k < 2; ++k) {
        Voice::CosmVoice& c = v->cs[k];
        c.holdWords();
        c.on = Z.cosm[k].on;
        c.stereo = stereo(xtCosmOsc(v->structure, k) ? v->osc2 : *v);   // the routines' flag (0x894eef70)
        if (Z.cosm[k].lfoKeySync) c.lfo.noteOn(clfoParams(*v, k), seed[3 + k], v->blockTempo);
        else if (other) c.lfo.noteOnFollow(clfoParams(*v, k), other->cs[k].lfo);
        else c.lfo.noteOnFree(clfoParams(*v, k), seed[3 + k], freeClfo[zone][k], v->blockTempo);
        if (!c.on) continue;
        c.type = Z.cosm[k].type;
        c.envA.noteOn(cosmEnvParams(*v, k, Z.cosm[k].envA));
        c.envB.noteOn(cosmEnvParams(*v, k, Z.cosm[k].envB));
        c.hold();
        if (c.type == 2)
            c.dual.noteOn(dualBlock(*v, k), c.envA.onsetLevel(), c.envA.level(), c.envA.smooth(), c.lfo.out(),
                          c.envB.onsetLevel(), c.envB.level(), c.envB.smooth(), c.lfo.out());
        else if (c.type == 3) {
            c.tb.noteOn(tbBlock(*v, k), c.envA.level(), c.lfo.out(), c.envB.level(), c.lfo.out());
            c.tb.setHpWords(c.word8c, c.word90);
        }
        else if (c.type == 7)
            c.res.noteOn(resBlock(*v, k), c.envA.onsetLevel(), c.envA.level(), c.envA.smooth(), c.lfo.out(),
                         c.envB.onsetLevel(), c.envB.level(), c.envB.smooth(), c.lfo.out());
        else if (c.type == 8)
            c.fs.noteOn(fsBlock(*v, k), c.envA.onsetLevel(), c.envA.level(), c.envA.smooth(), c.lfo.out(),
                        c.envB.onsetLevel(), c.envB.level(), c.envB.smooth(), c.lfo.out());
        else if (c.type == 9)
            c.lofi.noteOn(lofiBlock(*v, k), c.envA.onsetLevel(), c.envA.level(), c.envA.smooth(), c.lfo.out(),
                          c.envB.onsetLevel(), c.envB.level(), c.envB.smooth(), c.lfo.out());
        else if (c.type == 10)
            c.comp.noteOn(compBlock(*v, k));
        else if (c.type == 11)
            c.lim.noteOn(limBlock(*v, k));
        else if (c.type == 12)
            c.ws.noteOn(wsBlock(*v, k), c.envA.onsetLevel(), c.envA.level(), c.envA.smooth(), c.lfo.out(),
                        c.envB.onsetLevel(), c.envB.level(), c.envB.smooth(), c.lfo.out());
        else if (c.type == 13)
            c.od.noteOn(odBlock(*v, k), c.envA.onsetLevel(), c.envA.level(), c.envA.smooth(), c.lfo.out(),
                        c.envB.onsetLevel(), c.envB.level(), c.envB.smooth(), c.lfo.out());
        else if (c.type == 14)
            c.amp.noteOn(ampBlock(*v, k), c.envA.onsetLevel(), c.envA.level(), c.envA.smooth(), c.lfo.out());
        else if (c.type == 15)
            c.spk.noteOn({ Z.cosm[k].spkType });
        else if (c.type >= 4)
            ;                                          // below, once the voice's pitch is set
        else
            c.tvf.noteOn(cosmBlock(*v, k), c.envB.onsetLevel(), c.envB.level(), c.envB.smooth(), c.lfo.out(),
                         c.envA.onsetLevel(), c.envA.level(), c.envA.smooth(), c.lfo.out());
    }
    if (v->on) targets(*v, *v, Z, v->curA, v->curF, v->curWalk, v->curG, true);
    else v->curA = 1.0f;                               // the note-on of an OSC that is off (0x894f2f80, kind 0)
    if (v->osc2.on) targets(*v, v->osc2, Z.osc2, v->osc2.curA, v->osc2.curF, v->osc2.curWalk, v->osc2.curG, true);
    else v->osc2.curA = 1.0f;
    // the Comb and Side Band Filters' note-ons read the voice's pitch ratio
    // (+0x144) and root frequency (+0x9a4) as the voice's own note-on leaves
    // them (the tone note-on 0x894f5e60 runs the voice first): OSC 1's, also with
    // OSC 2 sounding, and for COSM 2 in the third Structure OSC 2's (xtCosmOsc); with
    // that OSC off a ratio of 1 and a root of 440
    for (int k = 0; k < 2; ++k) {
        Voice::CosmVoice& c = v->cs[k];
        if (!c.on || c.type < 4 || c.type > 6) continue;
        const bool second = xtCosmOsc(v->structure, k) != 0;
        const OscVoice& s = second ? v->osc2 : *v;
        const XtWaveTune tune = pcm(s) && s.wave ? s.wave->tune : XtWaveTune{};
        const bool at440 = s.robot || s.analog || s.extIn || !s.on;   // the analog and EXT IN voices' root is 440 too (0x894f21c0)
        const float root = xtCosmRootHz(second ? Z.osc2.rootKey : Z.rootKey, at440, tune.offset, tune.detune);
        if (c.type == 4)
            c.comb.noteOn(combBlock(*v, k), s.curA, root, c.envB.onsetLevel(), c.envB.level(), c.envB.smooth(),
                          c.lfo.out(), c.envA.onsetLevel(), c.envA.level(), c.envA.smooth(), c.lfo.out());
        else
            c.sbf.noteOn(sbfBlock(*v, k), s.curA, root, c.envB.onsetLevel(), c.envB.level(), c.envB.smooth(),
                         c.lfo.out(), c.envA.onsetLevel(), c.envA.level(), c.envA.smooth(), c.lfo.out());
    }
    // the voices' note-ons, OSC 1 then OSC 2 (0x894f21c0): SUPER-SAW's counter
    // and FEEDBACK-OSC's lines go in that order
    // LEGATO (0x894f6176…): each OSC whose earlier voice is of the same kind and mode takes its player, once its own note-on has run
    // (0x894f2d00) and not one at its wave's end, its +4 at 1 (0x894f2d24): a SOLO or band player's own flag, LITE's what it holds, and
    // Vari off's SOLO setup clears it, as the band and SOLO note-ons do, and nothing sets it there.  Vari Sw is the block's flag 0x20, not
    // the voice's mode, so across a Vari switch the XT finds a SOLO wave's two players alike and copies one into the other, which then reads
    // anywhere: here a player is taken only by its own kind (`linear`), as each player keeps its own seek (v151)
    const auto sameKind = [](const OscVoice& a, const OscVoice& b) {
        return a.on == b.on && a.analog == b.analog && a.extIn == b.extIn && a.lite == b.lite && a.linear == b.linear
               && (a.analog || a.extIn || !a.on || a.wave->type == b.wave->type);
    };
    const auto atEnd = [](const OscVoice& s) {
        const int f = s.lite ? s.staleEnd : s.linear ? 0 : s.wave->type == 1 ? s.solo.endFlag() : s.mb.endFlag();
        return (int8_t)f == 1;
    };
    if (v->on) oscStart(*v, *v, Z);
    if (legato1 && sameKind(*v, *prev) && !atEnd(*prev)) legatoTake(*v, *prev);
    if (v->osc2.on) oscStart(*v, v->osc2, Z.osc2);
    if (legato2 && sameKind(v->osc2, prev->osc2) && !atEnd(prev->osc2)) legatoTake(v->osc2, prev->osc2);
    // portamento (0x894f2440, after the voice setup): the pitch ramp from the
    // source key's cents to the note's; it moves from the next update on
    auto portaStart = [&](OscVoice& s, const OscControls& o) {
        s.kfKey = keyFollowKey(*v, o);
        s.porta.start(noteCents(*v, s, o, srcKey), noteCents(*v, s, o, note),
                      portaSettings(*v));
    };
    if (v->on) portaStart(*v, Z);
    if (v->osc2.on) portaStart(v->osc2, Z.osc2);
    // the MOD (0x894f1b00, 0x894f1d90)
    v->mod.noteOn(modBlk[zone], stereo(*v), stereo(v->osc2), v->on && v->analog, modShared, (int)(v - voice));
}
