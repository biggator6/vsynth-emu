// What XtSynth's voice keeps across notes, into the firmware's note (XtToneCarry.h).  Moved from XtToneCheck.cpp as it
// stood (v193).
#ifdef XT_TONE_MAIN
#include "XtToneCarry.h"
#include <cstring>

std::vector<VField> anaFields(const XtAnalogOsc& o) {
    std::vector<VField> f = { { "ratio", 0x144, o.ratio() }, { "gain target", 0x154, o.gainTarget() }, { "gain", 0x158, o.gain() }, { "fat target", 0x1c0, o.fatTarget() }, { "fat", 0x1c4, o.fat() },
        { "pw target", 0x1c8, o.pwTarget() }, { "pw", 0x1cc, o.pw() }, { "decay", 0x1d0, o.decay() }, { "phase", 0x1d4, o.phase() }, { "impact", 0x1d8, o.impact() }, { "edge", 0x1e0, o.edge() },
        { "hq level", 0x1e4, o.hqLevel() }, { "sub pw target", 0x1ec, o.subPwTarget() }, { "sub pw", 0x1f0, o.subPw() }, { "la step", 0x1f4, o.laStep() }, { "sub step", 0x1f8, o.subStep() },
        { "sub phase", 0x1fc, o.subPhase() }, { "sub level target", 0x200, o.subLevelTarget() }, { "sub level", 0x204, o.subLevel() }, { "ramp", 0x208, o.rampEnd() }, { "ramp 2", 0x210, o.rampEnd2() },
        { "la phase 2", 0x218, o.laPhase2() }, { "la phase 3", 0x21c, o.laPhase3() }, { "la steep target", 0x224, o.laSteepTarget() }, { "la steep", 0x228, o.laSteep() },
        { "la steep 2 target", 0x22c, o.laSteep2Target() }, { "la steep 2", 0x230, o.laSteep2() }, { "steep target", 0x234, o.steepTarget() }, { "steep", 0x238, o.steep() },
        { "steep 2 target", 0x23c, o.steep2Target() }, { "steep 2", 0x240, o.steep2() }, { "detune target", 0x24c, o.detuneTarget() }, { "detune", 0x250, o.detune() },
        { "mix target", 0x254, o.mixTarget() }, { "mix", 0x258, o.mix() }, { "fb state", 0x274, o.fbState() } };
    static const char* side[6] = { "side 0", "side 1", "side 2", "side 3", "side 4", "side 5" };
    for (int i = 0; i < 6; ++i) f.push_back({ side[i], 0x25c + 4 * (uint32_t)i, o.sidePhase(i) });
    return f;
}

void xtToneCarry(Firmware& fw, FwNote& n, const XtVoice* pv, int index, const XtControls& ctl) {
                // the Dynamic TVF's follower, which its note-on does not clear
                for (int k = 0; k < 2; ++k) { const float lev = pv->cs[k].tvf.level(); uint32_t u; std::memcpy(&u, &lev, 4); fw.c.wr32(n.TN + (k ? 0x14ec : 0x1388) + 0x58 + 0x98, u); }
                // the two words the stereo TB Filter reads uncleared
                for (int k = 0; k < 2; ++k) {
                    float w8c, w90;
                    pv->cs[k].wordsNow(w8c, w90);
                    uint32_t u;
                    std::memcpy(&u, &w8c, 4); fw.c.wr32(n.TN + (k ? 0x14ec : 0x1388) + 0x58 + 0x8c, u);
                    std::memcpy(&u, &w90, 4); fw.c.wr32(n.TN + (k ? 0x14ec : 0x1388) + 0x58 + 0x90, u);
                }
                // FM's lines are by the tone's number, and a far tap reads the next tone's
                fw.c.wr32(n.TN + 0x1708 + 0x3c, (uint32_t)index);
                // the MOD's, which a note-on leaves: ENV RING's last level above all
                const auto wrf = [&](uint32_t at, float x) { uint32_t u; std::memcpy(&u, &x, 4); fw.c.wr32(at, u); };
                const uint32_t M = n.TN + 0x1708;
                for (int i = 0; i < 3; ++i) wrf(M + 0x18 + 4 * (uint32_t)i, pv->mod.current(i));
                wrf(M + 0x28, pv->mod.env());
                wrf(M + 0x2c, pv->mod.lastLevel());
                fw.c.wr32(M + 0x30, (uint32_t)pv->mod.writeIndex());
                fw.c.wr32(M + 0x40, pv->mod.syncOn() ? 1 : 0);   // an OSC SYNC note leaves its flag set for the next
                // an LFO's clock fields, which only a note value sets, and with OSC 1 off the tempo its block had
                const XtLfo* lfos[5] = { &pv->lfo, &pv->osc2.lfo, &pv->cs[0].lfo, &pv->cs[1].lfo, &pv->tlfo };
                for (uint32_t l = 1; l <= 5; ++l) {
                    const XtLfo& q = *lfos[l - 1];
                    const uint32_t O = Firmware::lfoAt(n, l);
                    wrf(O + 0x84, q.clockStep()); wrf(O + 0x88, q.clockTarget()); wrf(O + 0x8c, q.clockTime());
                    fw.c.wr32(O + 0x90, (uint32_t)q.clockWrap()); fw.c.wr32(O + 0x94, (uint32_t)q.clockCount());
                }
                if (!ctl.on) wrf(n.BL + 0x494, pv->blockTempo);
                for (int o = 0; o < 2; ++o) {
                    if ((o ? ctl.osc2.oscType : ctl.oscType) != 0) continue;
                    const XtAnalogOsc& a = o ? pv->osc2.ana : pv->ana;
                    const uint32_t V = Firmware::voice(n, o);
                    for (const VField& f : anaFields(a)) { uint32_t u; std::memcpy(&u, &f.v, 4); fw.c.wr32(V + f.off, u); }
                    fw.c.wr8(V + 0x248, (uint8_t)a.noiseHold());
                    fw.c.wr32(V + 0x1dc, a.flags()); fw.c.wr32(V + 0x244, a.noiseState());
                    fw.c.wr32(V + 0x1e8, (uint32_t)a.hqFalling()); fw.c.wr32(V + 0x278, (uint32_t)a.fbWrite());
                    // the two limits the analog note-ons read, 0 after an analog note.  A LITE note with Vari on leaves its first sample and its
                    // backward loop's start there (its player's +0x138, +0x140), which an analog note on that voice then takes on the XT
                    fw.c.wr32(V + 0x20c, 0); fw.c.wr32(V + 0x214, 0);
                }
}
#endif
