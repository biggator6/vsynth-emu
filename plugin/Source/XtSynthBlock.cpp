// mixBlock, a block of 32 frames: the clocks, then each voice's tone update, its render by its Structure Type and its
// TVA onto the bus (moved from XtSynth.cpp as it stood, v200, and split into those parts; the EXT IN OSC's in them).
#include "XtSynth.h"
#include "XtSynthConstants.h"
#include "XtFloat.h"

#include <algorithm>
#include <cmath>
#include <iterator>

using namespace xtsynth;

namespace {
// 0x894f20d2: a released Time Trip going back to the beat, once silent: false until it is, then the player seeks the beat.
// (The XT picks a SOLO wave's seek by Vari Sw as it stands, 0x894f2ea4.  Switched during the note, that runs one player's seek
// on the other's state, which leaves it reading anywhere: here each player keeps its own.)
bool backOnBeat(XtOscVoice& s) {
    if (s.linear) {
        if (s.lin.gainNow() > kSilent) return false;
        s.lin.seek(s.lin.beat().beatPos >> 5);
    } else if (s.lite) {
        if (s.xfg.gainNow() > kSilent) return false;
        s.xfg.seek(s.xfg.beat().beatPos >> 5);
    } else if (s.wave->type == 1) {
        if (s.solo.gainNow() > kSilent) return false;
        s.solo.seek(s.solo.beat().beatPos >> 5);
    } else {
        if (s.mb.gainNow() > kSilent) return false;
        s.mb.seek(s.mb.beat().beatPos >> 5);
    }
    s.chase = false;
    return true;
}
}  // namespace

void XtSynth::mixBlock(float* lr) {
    std::fill(lr, lr + 64, 0.0f);
    std::fill(std::begin(frame), std::end(frame), 0.0f);
#ifdef XT_TONE_CHECK
    int32_t test[64];                                // xt_tone's input: what its firmware's tone has this block (XtExtIn.h)
    xtExtInTestBlock(hostFrames, test);
    input.push(test, 32);
#endif
    const int32_t* in = input.block();               // the block's input, for the EXT IN OSCs: taken every block, as it comes
    fxTempo.store(ctl.patchTempo, std::memory_order_relaxed);
    fxSix.store(clockExt ? clockSix : 0, std::memory_order_relaxed);
    // The tempo clock, here on the blocks; ahead of the tone update, as one choice of the two
    // tasks' order on the XT.  From outside: the timer's ms in this block first, then its clocks
    if (clockExt) {
        for (int n = msDue(); n > 0; --n) clockMs();
        for (int n = clocksDue(); n > 0; --n) { clockIn(); clockTick(); ++hostDone; }
    } else {
        for (int n = clocksDue(); n > 0; --n) { clockTick(); clockAcc -= (uint64_t)clockPeriod() * 44100; }
        clockAcc += kClockPerBlock;
    }
    arpBlock();                                   // the arpeggio's ms, then the ms timer on (XtSynthArp.cpp)
    hostAt += hostStep();
    ++clockBlocks;
    // The control clock: the tone update runs between blocks every 441 frames, and
    // the free-running LFOs step with it, a tone sounding or not.
    const bool tick = tickCount >= kTickFrames;
    if (tick) tickCount -= kTickFrames;
    tickCount += 32;
    if (tick) {
        for (int z = 0; z < kZones; ++z) {
            freeLfo[z].tick(1);
            freeLfo2[z].tick(1);
            for (XtLfoFree& f : freeClfo[z]) f.tick(1);
            freeTlfo[z].tick(1);
        }
    }
    if (!cur && std::none_of(std::begin(voice), std::end(voice), [](const Voice& v) { return v.active; })) { busOut(lr); return; }   // the effects' tails go on
    float tmp[64], tmp2[64];
    for (Voice& v : voice) {
        if (!v.active) continue;
        if (tick) toneUpdate(v);
        if (!v.active) continue;
        voiceRender(v, tmp, tmp2, in);
        voiceBus(v, tmp);
    }
    busOut(lr);
}

void XtSynth::toneUpdate(Voice& v) {
            // the tone update (0x894f75f0): envelopes step, the voice update
            // moves the player 70% toward its targets (0x8950c1b0), then the TVA
            v.env.tick();
            v.tlfo.tick(1);
            // An OSC's four envelopes and its LFO step only bound (0x894f7638: each +0x68, +0x80), which an EXT IN OSC's are not:
            // they hold where their note-on left them (v200)
            auto oscTick = [](OscVoice& s) { s.penv.tick(); s.tenv.tick(); s.fenv.tick(); s.oenv.tick(); s.lfo.tick(1); };
            if (v.on && !v.extIn) oscTick(v);
            if (v.osc2.on && !v.osc2.extIn) oscTick(v.osc2);
            for (Voice::CosmVoice& c : v.cs)
                if (c.on) { c.envA.tick(); c.envB.tick(); c.lfo.tick(1); }
            auto oscUpdate = [&](OscVoice& s, const OscControls& o) {
                if (s.chase && !backOnBeat(s)) return;        // 0x894f20d2: going back to the beat, once silent
                s.porta.tick(1);                               // 0x894f4b00, before the voice update
                float A, F, w, g;
                targets(v, s, o, A, F, w, g);
                const float gt = gainTarget(v, s, o, s.oenv.level());
                if (s.analog) {   // the analog update (0x89505400): the pitch at its target, no glide
                    s.curA = A;
                    s.ana.update(analogBlock(v, s, o), A, gt, s.fenv.level(), s.tenv.level(), s.lfo.out());
                } else if (s.extIn) {   // kind 4's update (0x89509780): the pitch and the gain's target as they are
                    s.curA = A;
                    s.curGain = gt;
                    s.ext.setTarget(gt);
                } else if (s.lite) {   // kind 1's update (0x8950bbb0): the values as they are, and no Energy
                    s.curA = A; s.curF = F; s.curWalk = w; s.curGain = gt;
                    applyRates(s);
                } else {
                    s.curA = xtOscSmooth(s.curA, A);
                    s.curF = xtOscSmooth(s.curF, F);
                    s.curWalk = xtOscSmooth(s.curWalk, w);
                    s.curG = xtOscSmooth(s.curG, g);
                    s.curGain = xtOscSmooth(s.curGain, gt);
                    applyRates(s);
                }
            };
            if (v.on) oscUpdate(v, zc(v));
            if (v.osc2.on) oscUpdate(v.osc2, zc(v).osc2);
            // the COSM updates (0x894ee6a0, COSM 1 then 2), after the voices', before the TVA
            for (int k = 0; k < 2; ++k) {
                Voice::CosmVoice& c = v.cs[k];
                if (!c.on) continue;
                c.hold();
                c.update(xtCosmOsc(v.structure, k) ? v.osc2.curA : v.curA, c.envA.level(), c.envA.smooth(), c.envB.level(), c.envB.smooth(), c.lfo.out());
            }
            tvaUpdate(v);
}

void XtSynth::voiceRender(Voice& v, float* tmp, float* tmp2, const int32_t* in) {
        // A PCM OSC's player, in the XT's units (int16 × 65536); the players' gains are the firmware's
        auto pcmBlock = [](OscVoice& s, float* lr) {
            if (s.linear) s.lin.block(lr);
            else if (s.lite) s.xfg.block(lr);
            else if (s.wave->type == 1) s.solo.renderXt(lr, 32);
            else s.mb.block(lr);
        };
        // The tone's render by its Structure Type (xtToneOrder), all in the XT's units
        uint32_t mask2 = 0;
#ifdef XT_TONE_CHECK
        bool modDone = false;
#endif
        for (const XtToneStep* st = xtToneOrder(v.structure); *st != XtToneStep::End; ++st) {
            switch (*st) {
            case XtToneStep::Osc2:
                if (!v.osc2.on) std::fill(tmp2, tmp2 + 64, 0.0f);
                else if (v.osc2.analog) mask2 = v.osc2.ana.process(tmp2);
                else if (v.osc2.extIn) v.osc2.ext.block(tmp2, in);   // kind 4's player (0x895097d0): the input at its gain
                else pcmBlock(v.osc2, tmp2);                   // a PCM OSC's mask is 0
                break;
            case XtToneStep::Cosm2OnOsc2:
                // what follows OSC 2's buffer is the mix's, as the last tone left it
                if (v.cs[1].on) v.cs[1].process(tmp2, 1.0f, mixPast);
                break;
            case XtToneStep::Osc1: {
                // While the MOD's flag is set OSC 2's mask goes to OSC 1's sync input
                // and to the MOD
                uint32_t sync = 0;
                if (v.mod.syncOn()) {
                    sync = mask2;
                    v.mod.setMask(mask2);
                }
                if (!v.on) std::fill(tmp, tmp + 64, 0.0f);
                else if (v.analog) v.ana.process(tmp, sync, tmp2);   // X-MOD reads OSC 2's buffer and clears it
                else if (v.extIn) v.ext.block(tmp, in);
                else pcmBlock(v, tmp);
                break;
            }
            case XtToneStep::Mod:
                // The MOD (0x894f1d50).  Off, or MIX at gains of 1: the sum.  A COSM on
                // an OSC that is off can still put something in its buffer
                if (v.mod.plain()) {
                    if (v.osc2.on || (v.structure == 2 && v.cs[1].on)) xtAnalogMix(tmp, tmp2);
                } else v.mod.process(tmp, tmp, tmp2);
                break;
            case XtToneStep::Cosm1:
                // on OSC 1's own buffer (Types 2, 3) what follows the block is OSC 2's; on the mix
                // it is the bus's first pairs, zeros with Output Assign MFX
                if (v.cs[0].on) v.cs[0].process(tmp, 1.0f, v.structure != 0 ? tmp2 : nullptr);
                break;
            case XtToneStep::Cosm2:
                if (v.cs[1].on) v.cs[1].process(tmp, 1.0f);
                break;
            case XtToneStep::End:
                break;
            }
#ifdef XT_TONE_CHECK
            if (*st == XtToneStep::Mod) { modDone = true; std::copy(tmp, tmp + 64, tapMix); }
            if (!modDone) { std::copy(tmp, tmp + 64, tapOsc1); std::copy(tmp2, tmp2 + 64, tapOsc2); }
#endif
        }
#ifdef XT_TONE_CHECK
        std::copy(tmp, tmp + 64, tapCosm);
#endif
        std::copy(tmp, tmp + 3, mixPast);
}

void XtSynth::voiceBus(Voice& v, const float* tmp) {
        // A tone nearly always goes on MULTI alone, and a level of 0 adds nothing to the bus.  So the other six only when one of them is in use.
        const auto otherBuses = [&v] {
            for (const int b : { XtBusMainL, XtBusMainR, XtBusReverb, XtBusChorus, XtBusDirL, XtBusDirR }) if (v.bus[b] != 0.0f || v.tb.bus[b] != 0.0f) return true;
            return false;
        };
        const bool others = otherBuses();
        // One sample on the bus: the channels through the gain as a and b; the sends take their mean.
        const auto onBus = [&v, others](float* f, float a, float b) {
            f[FrameMultiL] = xtf::add(f[FrameMultiL], xtf::mul(a, v.bus[XtBusMultiL]));
            f[FrameMultiR] = xtf::add(f[FrameMultiR], xtf::mul(b, v.bus[XtBusMultiR]));
            if (!others) return;
            f[FrameMainL] = xtf::add(f[FrameMainL], xtf::mul(a, v.bus[XtBusMainL]));
            f[FrameMainR] = xtf::add(f[FrameMainR], xtf::mul(b, v.bus[XtBusMainR]));
            f[FrameDirL] = xtf::add(f[FrameDirL], xtf::mul(a, v.bus[XtBusDirL]));
            f[FrameDirR] = xtf::add(f[FrameDirR], xtf::mul(b, v.bus[XtBusDirR]));
            const float mean = xtf::mul(xtf::add(a, b), 0.5f);
            f[FrameReverb] = xtf::add(f[FrameReverb], xtf::mul(mean, v.bus[XtBusReverb]));
            f[FrameChorus] = xtf::add(f[FrameChorus], xtf::mul(mean, v.bus[XtBusChorus]));
        };
        if (v.damping) {
            // truncated (0x894f5570): the gain and the bus's levels held, times a fade that
            // falls by 1/(Truncate Time·44.1) each sample; silent and done at 2^-16
            for (int i = 0; i < 32; ++i) {
                v.damp = xtf::sub(v.damp, truncateStep());
                if (!(v.damp > kSilent)) { v.damp = 0.0f; v.active = false; break; }
                onBus(frame + kFrameWords * i, xtf::mul(xtf::mul(tmp[2 * i], v.cur[0]), v.damp), xtf::mul(xtf::mul(tmp[2 * i + 1], v.cur[1]), v.damp));
            }
            return;
        }
        // TVA render (0x894f5720): for each channel a step that would pass the target
        // within the block is cut to (target − gain)/32.5; gain applied, then stepped.
        // The output bus takes that at its own levels, each moving by a share of the gap as
        // the block began.  DIR's never move: they start at the block's, and that never changes.
        float s[2], e[XtBusDirL];
        for (int c = 0; c < 2; ++c) {
            const float d = xtf::sub(v.target[c], v.cur[c]);
            s[c] = std::fabs(xtf::mul(v.step[c], 32.0f)) > std::fabs(d) ? xtf::mul(d, kLand) : v.step[c];
        }
        for (int b = 0; b < XtBusDirL; ++b) e[b] = xtf::mul(xtf::sub(v.tb.bus[b], v.bus[b]), kSendGlide);
        for (int i = 0; i < 32; ++i) {
            onBus(frame + kFrameWords * i, xtf::mul(tmp[2 * i], v.cur[0]), xtf::mul(tmp[2 * i + 1], v.cur[1]));
            for (int c = 0; c < 2; ++c) v.cur[c] = xtf::add(v.cur[c], s[c]);
            v.bus[XtBusMultiL] = xtf::add(v.bus[XtBusMultiL], e[XtBusMultiL]);
            v.bus[XtBusMultiR] = xtf::add(v.bus[XtBusMultiR], e[XtBusMultiR]);
            if (others)
                for (const int b : { XtBusMainL, XtBusMainR, XtBusReverb, XtBusChorus }) v.bus[b] = xtf::add(v.bus[b], e[b]);
        }
        v.elapsed += 32.0f;
}
