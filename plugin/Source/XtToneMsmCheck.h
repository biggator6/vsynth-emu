#pragma once
// xt_tone's MSM mode (XtToneMsm.cpp, XT_TONE_MAIN only, v193): what it reads of XtSynth, its tally, and what it compares
// each block.
#include <cstdio>
#include <cstring>
#include "XtSwitches.h"
#include "XtToneCarry.h"
#include "XtToneFwMsm.h"

// What differs, counted, and with XT_TONE_FIRST said for the first few: by case and block, a name and its prefix
struct Tally {
    static constexpr int kSaid = 12;
    long cases = 0;
    long blocks = 0;
    long notes = 0;
    long updates = 0;
    long checks = 0;
    long different = 0;
    long said = 0;
    int g = 0;
    int blk = 0;
    const char* prefix = "";
    bool first = xtSwitchIs("XT_TONE_FIRST", "1");
    void tell(const char* what, bool equal, double a, double b) {
        ++checks;
        if (equal) return;
        ++different;
        if (!first || said >= kSaid) return;
        ++said;
        std::printf("  case %d block %d: %s%s: fw %.9g port %.9g\n", g, blk, prefix, what, a, b);
    }
    void same(const char* what, double a, double b) { tell(what, a == b, a, b); }
    // Two floats as xt_tone compares them: equal (a zero's sign aside), or the same bits (a NaN)
    void bits(const char* what, float a, float b) { tell(what, a == b || std::memcmp(&a, &b, 4) == 0, a, b); }
    // Two floats bit for bit, a zero's sign too: the bus's words
    void exact(const char* what, float a, float b) { tell(what, std::memcmp(&a, &b, 4) == 0, a, b); }
};

// XtSynth's own, as a friend of the class under XT_TONE_CHECK
struct XtToneMsmProbe {
    static int msDue(const XtSynth& s) { return s.msDue(); }
    static int clocksDue(const XtSynth& s) { return s.clocksDue(); }
    static bool tick(const XtSynth& s) { return s.tickCount >= 441; }
    static long frames(const XtSynth& s) { return s.hostFrames; }
    static float word(const XtSynth& s, int i) { return s.frame[i]; }
    static const XtMsm& msm(const XtSynth& s) { return s.msm; }
    static const XtVoice& voice(const XtSynth& s, int i) { return s.voice[i]; }
    // Envelope e as xt_tone numbers them: 1…8 the OSCs' pitch, time, formant and amp, 9…12 the COSMs', 13 the TVA's
    static const XtEnvelope& env(const XtVoice& v, int e) {
        if (e == 13) return v.env;
        if (e >= 9) return (e - 9) % 2 ? v.cs[(e - 9) / 2].envB : v.cs[(e - 9) / 2].envA;
        const XtSynth::OscVoice& o = e <= 4 ? static_cast<const XtSynth::OscVoice&>(v) : v.osc2;
        const XtEnvelope* const four[4] = { &o.penv, &o.tenv, &o.fenv, &o.oenv };
        return *four[(e - 1) % 4];
    }
    // A note's OSCs, those that are on: the LFO's output, the cents at its key and the source's, the pitch knob, the
    // glide's ramp as 0x894f4c00 reads it, and an analog oscillator's own (as xt_tone compares them)
    static void oscs(Tally& t, Firmware& fw, const FwNote& n, const XtSynth& s) {
        static const char* const kOsc[2] = { "osc 1 ", "osc 2 " };
        const XtVoice& v = s.voice[n.vi];
        for (int o = 0; o < 2; ++o) {
            const XtSynth::OscVoice& os = o ? v.osc2 : static_cast<const XtSynth::OscVoice&>(v);
            if (!os.on) continue;
            const XtSynth::Controls& z = s.zc(v);
            const XtSynth::OscControls& oc = o ? z.osc2 : static_cast<const XtSynth::OscControls&>(z);
            const uint32_t V = Firmware::voice(n, o);
            const uint32_t RP = V + 0x880;
            t.prefix = kOsc[o];
            t.bits("lfo output", fw.f32(Firmware::lfoAt(n, (uint32_t)o + 1) + 0x38), os.lfo.out());
            t.bits("lfo phase", fw.f32(Firmware::lfoAt(n, (uint32_t)o + 1) + 0x40), os.lfo.phase());
            t.bits("cents", fw.f32(V + 8), s.noteCents(v, os, oc, v.note));
            t.bits("source cents", fw.f32(V + 0x68), s.noteCents(v, os, oc, v.srcKey));
            t.bits("pitch knob", fw.f32(V + 0xc), s.pitchKnob(v, os, oc));
            t.bits("ramp", fw.c.rd32(RP + 4) ? fw.f32(RP + 0x3c) : 0.0f, os.porta.value());
            if (os.analog)
                for (const VField& f : anaFields(os.ana)) t.bits(f.name, fw.f32(V + f.off), f.v);
        }
        t.prefix = "";
    }
    // The last voice's buffers: the OSCs', the mix and the COSMs' output (as xt_tone compares them; the COSMs work in
    // place on the mix, so with one on the mix's own isn't kept)
    static void buffers(Tally& t, Firmware& fw, const XtSynth& s) {
        struct B {
            const char* name;
            uint32_t at;
            const float* p;
        };
        const B bs[] = { { "osc 2 buffer", kBuf2, s.tapOsc2 }, { "osc 1 buffer", kBuf1, s.tapOsc1 },
                         { "mix", kBufMix, s.tapMix }, { "cosm output", kBufMix, s.tapCosm } };
        const bool cosm = s.ctl.cosm[0].on || s.ctl.cosm[1].on;
        for (const B& b : bs) {
            if (b.p == s.tapMix && cosm) continue;
            for (int i = 0; i < 64; ++i) t.bits(b.name, fw.f32(b.at + 4 * (uint32_t)i), b.p[i]);
        }
    }
    // A Comb Filter's delays, Tone and crossfade (as xt_cosm compares them), the COSM's object at O, its data at +0x58
    static void comb(Tally& t, Firmware& fw, uint32_t O, const XtCosmComb& p) {
        const uint32_t D = O + 0x58;
        const VField f[] = {
            { "comb delay", D + 0xbc, p.delay() },
            { "comb tone", D + 0xc0, p.tone() },
            { "comb tone target", O + 0x100, p.toneTarget() },
            { "comb new delay", O + 0x130, p.newDelay() },
            { "comb fraction", O + 0x134, p.fraction() },
            { "comb new fraction", O + 0x138, p.newFraction() },
            { "comb progress", O + 0x13c, p.progress() },
        };
        for (const VField& x : f) t.bits(x.name, fw.f32(x.off), x.v);
    }
    // The channel's controllers as XtSynth has them at power-on (ccDefaults), into the firmware's channel
    static void controllers(const XtSynth& s, Firmware& fw) {
        for (int n = 0; n < 96; ++n) fw.setCc(n, s.cc[n]);
    }
    // The voice XtSynth's next note will take, its first free one, and its number
    static const XtVoice* next(const XtSynth& s, int& index) {
        for (index = 0; index < XtSynth::kVoices; ++index)
            if (!s.voice[index].active) return &s.voice[index];
        index = 0;
        return &s.voice[0];
    }
    // The tempo ticks the next block's ms will take (XtSynth::arpMs, INTERNAL), on a copy of its count
    static void ticks(const XtSynth& s, int ms, int32_t* d) {
        XtTempoTicks t = s.arpTicks;
        for (int i = 0; i < ms; ++i) {
            t.msInternal(s.clockPeriod());
            d[i] = t.take();
        }
    }
};

// A note: its TVA's level and LFO depths, gains, targets and steps, and the output bus's eight levels with the block's
// they move toward; its envelopes' output and stage, those of an OSC or COSM that's on; the TVA's and COSMs' LFOs; and
// its OSCs (as xt_tone compares them)
inline void compareNote(Tally& t, Firmware& fw, const FwNote& n, const XtSynth& s) {
    const XtVoice& v = XtToneMsmProbe::voice(s, n.vi);
    const uint32_t A = Firmware::tva(n);
    const VField tv[] = { { "tva level", 0xc, v.tb.level }, { "tva lfo depth", 8, v.tb.lfoDepth },
                          { "tva pan lfo depth", 0x1c, v.tb.panLfoDepth },
                          { "tva gain", 0x68, v.cur[0] }, { "tva gain r", 0x6c, v.cur[1] },
                          { "tva target", 0x60, v.target[0] }, { "tva target r", 0x64, v.target[1] },
                          { "tva step", 0x58, v.step[0] }, { "tva step r", 0x5c, v.step[1] } };
    for (const VField& f : tv) t.bits(f.name, fw.f32(A + f.off), f.v);
    static const char* const kBus[XtBusCount][2] = {
        { "tva bus level main l", "tva bus target main l" },   { "tva bus level main r", "tva bus target main r" },
        { "tva bus level multi l", "tva bus target multi l" }, { "tva bus level multi r", "tva bus target multi r" },
        { "tva bus level reverb", "tva bus target reverb" },   { "tva bus level chorus", "tva bus target chorus" },
        { "tva bus level dir l", "tva bus target dir l" },     { "tva bus level dir r", "tva bus target dir r" } };
    for (int b = 0; b < XtBusCount; ++b) {
        t.bits(kBus[b][0], fw.f32(A + 0x70 + 4 * (uint32_t)b), v.bus[b]);
        t.bits(kBus[b][1], fw.f32(A + 0x20 + 4 * (uint32_t)b), v.tb.bus[b]);
    }
    static const char* const kEnv[14] = { "", "env 1", "env 2", "env 3", "env 4", "env 5", "env 6", "env 7",
                                          "env 8", "env 9", "env 10", "env 11", "env 12", "env 13 (TVA)" };
    for (int e = 1; e <= 13; ++e) {
        const uint32_t at = Firmware::envAt(n, (uint32_t)e);
        if (e >= 9 && e <= 12 && (!v.cs[(e - 9) / 2].on || fw.c.rd32(at + 0x68) == 0)) continue;
        if (e <= 8 && !(e <= 4 ? v.on : v.osc2.on)) continue;
        const XtEnvelope& pe = XtToneMsmProbe::env(v, e);
        t.bits(kEnv[e], fw.f32(at + 0x4c), pe.level());
        t.same(kEnv[e], (int)fw.c.rd32(at), pe.stage());
    }
    t.bits("tva lfo output", fw.f32(Firmware::lfoAt(n, 5) + 0x38), v.tlfo.out());
    static const char* const kCosm[2][2] = { { "cosm 1 lfo output", "cosm 1 held level" },
                                             { "cosm 2 lfo output", "cosm 2 held level" } };
    for (int k = 0; k < 2; ++k) {
        const XtVoice::CosmVoice& c = v.cs[k];
        const uint32_t L = Firmware::lfoAt(n, 3 + (uint32_t)k);
        if (c.on && fw.c.rd32(L + 0x80) != 0) t.bits(kCosm[k][0], fw.f32(L + 0x38), c.lfo.out());
        if (!c.on) continue;
        // the levels a new block reads, by what the binding put in each slot
        const uint32_t O = n.TN + (k ? 0x14ec : 0x1388);
        const uint32_t a = Firmware::envAt(n, 9 + 2 * (uint32_t)k), b = Firmware::envAt(n, 10 + 2 * (uint32_t)k);
        for (uint32_t s = 0; s < 4; ++s) {
            const uint32_t bound = fw.c.rd32(O + 0x150 + 4 * s);
            if (bound != a && bound != b && bound != L) continue;
            t.bits(kCosm[k][1], fw.f32(O + 0x104 + 4 * s), bound == a ? c.heldA : bound == b ? c.heldB : c.heldLfo);
        }
        if (c.type == 4) XtToneMsmProbe::comb(t, fw, O, c.comb);
    }
    XtToneMsmProbe::oscs(t, fw, n, s);
}

// Each block's whole bus, word by word (8 a frame, 0x894f5720), and the two objects: running, and each track's state,
// step and term
inline void compare(Tally& t, Firmware& fw, const XtSynth& syn) {
    static const char* const kWord[8] = { "bus MAIN L", "bus MAIN R", "bus Reverb", "bus Chorus",
                                          "bus MULTI L", "bus MULTI R", "bus DIR L", "bus DIR R" };
    for (int i = 0; i < 32 * 8; ++i)
        t.exact(kWord[i % 8], fw.f32(kBuf + 4 * (uint32_t)i), XtToneMsmProbe::word(syn, i));
    const XtMsm& m = XtToneMsmProbe::msm(syn);
    t.same("msm running", fw.c.rd8(kFwMsmObj + 0x24), m.running);
    for (int n = 0; n < XtMsm::kTracks; ++n) {
        const uint32_t at = kFwMsmObj + 0x2c + 30 * (uint32_t)n;
        t.same("msm track state", fw.c.rd8(at), m.track[n].state);
        t.same("msm track step", (int8_t)fw.c.rd8(at + 1), m.track[n].cur);
        t.same("msm track term", (int8_t)fw.c.rd8(at + 0x1c), m.track[n].out);
        t.same("msm track clocks left", (int8_t)fw.c.rd8(at + 0x1d), m.track[n].count);
        t.same("msm track grid", fw.c.rd8(at + 7), m.track[n].grid);
    }
    t.same("msm clocks pending", (int32_t)fw.c.rd32(kFwMsmObj + 0x20), m.pending);
    t.same("msm tempo ticks", (int32_t)fw.c.rd32(FwMsm::kTimerGlobals[1]), m.now);
    t.same("msm timer set for", (int32_t)fw.c.rd32(kFwMsmObj + 0x1c), m.pos);
    t.same("msm timer due", (int32_t)fw.c.rd32(kFwMsmObj + 8), m.due);
}
