// The XT's envelope generator — see XtEnvelope.h.  Single precision as the
// SH-4 computes it (XtFloat.h).
#include "XtEnvelope.h"
#include "XtFloat.h"

#include <cmath>
#include <cstdio>

namespace {

constexpr uint32_t kRomBase = 0x89300000;
constexpr uint32_t kTimeTab = 0x897cec24;     // u16 ×128: knob → ms (0x894df7b0)
constexpr uint32_t kSustainTab = 0x897cfcb8;  // u16 ×128: knob → level ×32768 (0x894df240)
constexpr uint32_t kAttackTab = 0x897d1934;   // u16 ×257: attack curve (0x894ef7d0)
constexpr uint32_t kShapeTab = 0x897d1b36;    // u16 ×129 (+ what follows): segment shape (0x894ef960)
constexpr uint32_t kDepthTab = 0x897cef44;    // u8 ×13: pitch depth 0..12 in cents (0x894e0ce0)

struct Tables {
    uint16_t time[128], sustain[128], attack[257], shape[257];
    uint8_t depth[13];
    uint32_t keyScale[41][128];
    bool ok = false, keyScales = false;
} T;

template <class V> bool readAt(FILE* f, uint32_t addr, V* v, size_t n) {
    return std::fseek(f, (long)(addr - kRomBase), SEEK_SET) == 0 && std::fread(v, sizeof(V), n, f) == n;
}

} // namespace

bool xtEnvLoadTables(const char* romPath) {
    T.ok = false;
    FILE* f = romPath ? std::fopen(romPath, "rb") : nullptr;
    if (!f) return false;
    const bool ok = readAt(f, kTimeTab, T.time, 128) && readAt(f, kSustainTab, T.sustain, 128)
                 && readAt(f, kAttackTab, T.attack, 257) && readAt(f, kShapeTab, T.shape, 257)
                 && readAt(f, kDepthTab, T.depth, 13);
    std::fclose(f);
    if (!ok) return false;
    // Sanity: the three tables rise from 0 (time and sustain strictly).
    for (int i = 1; i < 128; ++i)
        if (T.time[i] <= T.time[i - 1] || T.sustain[i] <= T.sustain[i - 1]) return false;
    for (int i = 1; i < 257; ++i)
        if (T.attack[i] < T.attack[i - 1]) return false;
    for (int i = 1; i < 129; ++i)
        if (T.shape[i] < T.shape[i - 1]) return false;
    if (T.time[0] != 0 || T.sustain[0] != 0 || T.sustain[127] != 32768 || T.attack[0] != 0
        || T.shape[0] != 0 || T.shape[128] != 65535 || T.depth[0] != 0) return false;
    for (int i = 1; i < 13; ++i)
        if (T.depth[i] < T.depth[i - 1]) return false;
    T.ok = true;
    return true;
}

bool xtEnvTablesLoaded() { return T.ok; }

int xtEnvTime(int knob, int velocity, int sens, uint32_t scale) {
    int t = (int16_t)T.time[knob & 0x7f];               // mov.w: sign-extended
    if (t == 0) return 0;
    sens = (int8_t)sens;
    if (sens != 0) {
        // sens·256000 / 6400 by a reciprocal multiply, rounded toward zero
        const int64_t prod = (int64_t)(sens * 256000) * (int64_t)0x51eb851f;
        int q = (int32_t)(prod >> 32) >> 11;
        q += q < 0;
        int u = (t & 0xffff) - (((velocity & 0xff) - 64) * q >> 8);
        if (u <= 0) return 0;
        t = u > 12000 ? 12000 : u;
    }
    if (scale == 0x10000) return t & 0xffff;
    uint32_t u;
    if (scale < 0x10000) u = ((uint32_t)(t & 0xffff) * (scale & 0xffff)) >> 16;
    else {
        u = (((uint32_t)(t & 0xffff) * (scale & 0xffff)) >> 16) + (uint32_t)(t & 0xffff) * ((scale >> 16) & 0xffff);
        if (u > 12000) u = 12000;
    }
    if ((u & 0xffff) == 0) u = 1;
    return (int)(u & 0xffff);
}

void xtEnvSetKeyScales(const uint32_t* t) {
    for (int i = 0; i < 41; ++i)
        for (int k = 0; k < 128; ++k) T.keyScale[i][k] = t[i * 128 + k];
    T.keyScales = true;
}

uint32_t xtEnvKeyScale(int key, int kf) {
    if (T.keyScales && kf % 10 == 0 && kf >= -200 && kf <= 200 && key >= 0 && key < 128) return T.keyScale[(kf + 200) / 10][key];
    const float e = xtf::div(xtf::flt((key - 60) * -(int)(int16_t)kf), 1200.0f);
    const double v = std::pow(2.0, (double)e) * 65536.0;   // the firmware's powf, then ×65536
    return v >= 2147483648.0 ? (uint32_t)(int64_t)(v - 4294967296.0) : (uint32_t)(int32_t)v;
}

float xtPitchDepthCents(float d) {
    using namespace xtf;
    const float s = d < 0.0f ? -1.0f : 1.0f;
    const float a = d < 0.0f ? -d : d;
    auto line = [&](float base, float per) { return mul(add(mul(a, per), base), s); };   // fmac, then the sign
    if (!(a > 12.0f)) return mul(flt(T.depth[ftrc(a)]), s);
    if (!(a > 36.0f)) return line(-600.0f, 50.0f);
    if (!(a > 48.0f)) return line(-2400.0f, 100.0f);
    if (!(a > 60.0f)) return line(-7200.0f, 200.0f);
    return line(-19200.0f, 400.0f);
}

XtEnvParams xtEnvAdsr(const XtEnvKnobs& k, int key, int velocity, int releaseVelocity, int curve) {
    auto knob = [](int v) { return v < 0 ? 0 : v > 127 ? 127 : v; };
    // the envelope table stores each time sens as trunc(knob·100/63) (0x894df8b0)
    auto sens = [](int s) { s = s < -63 ? -63 : s > 63 ? 63 : s; return s * 100 / 63; };
    const uint32_t kfScale = xtEnvKeyScale(key, k.timeKF);
    XtEnvParams p;
    p.curve = curve;
    p.time[0] = (float)xtEnvTime(knob(k.attack), velocity, sens(k.aSens), 0x10000);
    p.time[1] = 0.0f;
    p.time[2] = (float)xtEnvTime(knob(k.decay), velocity, sens(k.dSens), kfScale);
    p.time[3] = (float)xtEnvTime(knob(k.release), releaseVelocity, sens(k.rSens), kfScale);
    p.level[0] = 0.0f;
    p.level[1] = p.level[2] = 1.0f;
    p.level[3] = xtf::div((float)T.sustain[knob(k.sustain)], 32768.0f);
    p.level[4] = 0.0f;
    return p;
}

// 0x894ef520: phase step per tick.  A stage of T ms takes T/10 ticks; 10 ms
// or less, one tick.  The release ignores the time scale.
int XtEnvelope::stageRate() const {
    float t;
    switch (st) {
    case 0: t = p.time[0]; break;
    case 1: t = p.time[1]; break;
    case 2: t = p.time[2]; break;
    case 4: t = p.time[3]; break;
    default: return 0;
    }
    const int ti = xtf::ftrc(t);
    const int r = ti > 10 ? 655360 / ti : 0x10000;
    return st == 4 ? r : xtf::ftrc(xtf::mul(xtf::flt(r), p.scale));
}

// 0x894ef7d0: the output for the current stage and phase (linear; the
// attack follows the attack table for curve 2).
void XtEnvelope::linear() {
    const float* L = p.level;
    auto lerp = [&](float a, float b, int x) { return xtf::add(xtf::div(xtf::mul(xtf::sub(b, a), xtf::flt(x)), 65536.0f), a); };
    switch (st) {
    case 0:
        if (p.curve == 2) {
            const int i = (phase >> 8) & 0xffff, u = T.attack[i];
            out = lerp(L[0], L[1], (((phase & 0xff) * (T.attack[i + 1] - u)) >> 8) + u);
        } else out = lerp(L[0], L[1], phase);
        break;
    case 1: out = lerp(L[1], L[2], phase); break;
    case 2: out = lerp(L[2], L[3], phase); break;
    case 3: out = L[3]; break;
    case 4:
        out = lerp(rel, L[4], phase);
        if (std::fabs(out) <= 1.5258788e-05f) st = 5;
        break;
    case 5: out = L[4]; break;
    case 6: out = L[1]; break;
    default: break;
    }
}

// 0x894ef960 (curve 2, after the linear output): each segment is reshaped
// through the shape table — out = low + range·C(32767·(out − low)/range),
// mirrored where both of a segment's levels are negative.  The attack
// becomes C(attack curve), the decay and release exponential-like.
void XtEnvelope::shape() {
    using namespace xtf;
    const float* L = p.level;
    auto C = [](int x) {                              // 0x894efcc0
        x &= 0xffff;
        const int i = x >> 8, u = T.shape[i];
        return div(flt(((((x & 0xff) * (T.shape[i + 1] - u)) >> 8) + u) & 0xffff), 65536.0f);
    };
    auto pos = [](float rel, float range) { return ftrc(mul(div(rel, range), 32767.0f)); };
    auto common = [&](float base, float range, float rel) { out = add(mul(C(pos(rel, range)), range), base); };
    switch (st) {
    case 0:
        if (L[0] > L[1]) { const float r = sub(L[0], L[1]); out = sub(L[0], mul(C(0x7fff - pos(sub(out, L[1]), r)), r)); }
        else common(L[0], sub(L[1], L[0]), sub(out, L[0]));
        break;
    case 1:
    case 2: {
        const float a = L[st], b = L[st + 1];
        const float base = a > b ? b : a, range = a > b ? sub(a, b) : sub(b, a), rel = sub(out, base);
        if (a < 0.0f && b < 0.0f) out = sub(L[2], mul(C(0x7fff - pos(rel, range)), range));
        else common(base, range, rel);
        break;
    }
    case 3: out = L[3]; break;
    case 4:
        if (rel > L[4]) common(L[4], sub(rel, L[4]), sub(out, L[4]));
        else { const float r = sub(L[4], rel); out = sub(L[4], mul(C(pos(sub(L[4], out), r)), r)); }
        break;
    default: break;
    }
}

// The output function the curve type installs: 0x894ef7d0 (curve 0) or
// 0x894efca0 = 0x894ef7d0 then 0x894ef960 (curve 2).
void XtEnvelope::output() {
    linear();
    if (p.curve == 2) shape();
}

// 0x894efd00: stages shorter than a tick get a slower follower.
void XtEnvelope::setCoef() {
    if (p.curve == 1) return;
    coef = 1.0f / 32.0f;
    float t;
    switch (st) {
    case 0: t = p.time[0]; break;
    case 1: t = p.time[1]; break;
    case 2: t = p.time[2]; break;
    case 4: t = p.time[3]; break;
    default: return;
    }
    if (t < 10.0f) coef = xtf::div(xtf::sub(1.0f, xtf::div(t, 30.0f)), 32.0f);
}

// 0x894f0130
void XtEnvelope::noteOn(const XtEnvParams& prm) {
    p = prm;
    phase = 0;
    st = 0;
    stT = p.time[0];
    onset = p.time[0] == 0.0f ? p.level[1] : p.level[0];
    if (p.time[0] == 0.0f) {
        // no attack: straight into the (instant) second stage at its end
        st = 1;
        rate = stageRate();
        phase = 0xffff;
        output();
        setCoef();
    } else if (p.time[0] < 10.0f) {
        // shorter than a tick: the peak at once, the follower makes the ramp
        rate = 0xffff;
        out = p.level[1];
        coef = xtf::div(xtf::sub(1.0f, xtf::div(p.time[0], 30.0f)), 32.0f);
    } else {
        rate = stageRate();
        phase = rate;
        output();
        setCoef();
    }
    phase = rate;   // every path leaves with the phase one step on (0x894f03d0)
}

// 0x894f03e0
void XtEnvelope::noteOff() {
    st = 4;
    rel = out;
    rate = stageRate();
    phase = rate;
}

// 0x894f0080: a sustaining envelope moves to the new level at once.
void XtEnvelope::update(const XtEnvParams& prm) {
    for (int i = 0; i < 4; ++i) p.time[i] = prm.time[i];
    for (int i = 0; i < 5; ++i) p.level[i] = prm.level[i];
    p.scale = prm.scale;
    if (st == 3) {
        out = p.level[3];
        if (p.scale < 0.0f) { st = 2; phase = 0xffff; }
    } else if (st == 6 && p.scale > 0.0f) { st = 1; phase = 0; }
    rate = stageRate();
}

// 0x894efdf0
void XtEnvelope::tick(int n) {
    if (phase == 0 && st != 3) {
        if (st >= 5) return;
        ++st;
        rate = stageRate();
    }
    if (st < 0 || st > 4) return;
    static const int stageTimeIndex[5] = { 0, 1, 2, -1, 3 };
    stT = stageTimeIndex[st] < 0 ? -1.0f : p.time[stageTimeIndex[st]];
    if (st == 0 && p.scale < 0.0f) return;
    if (st != 3) rate = stageRate();
    const int ph = n * rate + phase;
    if (ph >= 0x10000) {
        phase = 0;
        rate = 0;
        static const int endLevel[6] = { 1, 2, 3, 3, 4, 4 };
        out = p.level[endLevel[st]];
        setCoef();
        return;
    }
    phase = ph & 0xffff;
    output();
    setCoef();
}

#ifdef XT_ENV_MAIN
// xt_env <rom.2.bin> <A> <D> <S> <R> <offTick> <ticks> [curve]
// Runs the firmware's envelope (constructor 0x894efdb0, note-on 0x894f0130,
// tick 0x894efdf0, note-off 0x894f03e0) in the SH-4 interpreter beside the
// port, one tick at a time, and prints the first difference.
#include "Sh4Interp.h"
#include <cstdlib>
#include <cstring>
int main(int argc, char** argv) {
    if (argc == 3 && std::string(argv[2]) == "conv") {
        // the time converter and the Time KF scale against the firmware, exhaustively
        if (!xtEnvLoadTables(argv[1])) return 1;
        Sh4 c;
        if (!c.loadRom(argv[1])) return 1;
        const uint32_t sp = 0x8C0F0000;
        c.mapRam(0x8C000000, 0x100000);
        long n = 0, bad = 0;
        for (int key = 0; key < 128; ++key)
            for (int kf = -200; kf <= 200; kf += 10) {
                uint32_t r0 = 0;
                if (!c.call(0x894df760, { (uint32_t)key, (uint32_t)kf }, sp, &r0)) { std::fprintf(stderr, "%s\n", c.error().c_str()); return 1; }
                ++n;
                if (r0 != xtEnvKeyScale(key, kf) && bad++ < 10) std::printf("kf scale key %d kf %d: fw %08x port %08x\n", key, kf, r0, xtEnvKeyScale(key, kf));
            }
        std::printf("Time KF scale, std::pow: %ld cases, %ld differ\n", n, bad);
        std::vector<uint32_t> tab(41 * 128);
        for (int i = 0; i < 41; ++i)
            for (int key = 0; key < 128; ++key) {
                uint32_t r0 = 0;
                c.call(0x894df760, { (uint32_t)key, (uint32_t)(-200 + 10 * i) }, sp, &r0);
                tab[(size_t)(i * 128 + key)] = r0;
            }
        xtEnvSetKeyScales(tab.data());
        bad = 0;   // the KF scale is not part of the time converter's verdict once the firmware's values are used
        const uint32_t scales[] = { 0x10000, 0x8000, 0x4a3d, 0xffff, 0x10001, 0x1a000, 0x28000, 0x40000, 0x1, 0x0 };
        n = bad = 0;
        for (int knob = 0; knob < 128; ++knob)
            for (int vel = 0; vel < 128; vel += 3)
                for (int sens = -63; sens <= 63; sens += 7)
                    for (uint32_t sc : scales) {
                        uint32_t r0 = 0;
                        if (!c.call(0x894df7b0, { (uint32_t)knob, (uint32_t)vel, (uint32_t)(int32_t)sens, sc }, sp, &r0)) { std::fprintf(stderr, "%s\n", c.error().c_str()); return 1; }
                        ++n;
                        const int fw = (int)(r0 & 0xffff), pt = xtEnvTime(knob, vel, sens, sc);
                        if (fw != pt && bad++ < 10) std::printf("time knob %d vel %d sens %d scale %x: fw %d port %d\n", knob, vel, sens, sc, fw, pt);
                    }
        std::printf("time converter: %ld cases, %ld differ\n", n, bad);
        long bad2 = 0;
        for (int d = -63; d <= 63; ++d) {
            // fr4 = depth; result in fr0
            c.fb[0][4] = 0; float fd = (float)d; std::memcpy(&c.fb[0][4], &fd, 4);
            if (!c.call(0x894e0ce0, {}, sp)) { std::fprintf(stderr, "%s\n", c.error().c_str()); return 1; }
            float fw; std::memcpy(&fw, &c.fb[0][0], 4);
            if (fw != xtPitchDepthCents(fd) && bad2++ < 10) std::printf("pitch depth %d: fw %g port %g\n", d, fw, xtPitchDepthCents(fd));
        }
        std::printf("pitch depth: 127 cases, %ld differ\n", bad2);
        return bad || bad2 ? 3 : 0;
    }
    if (argc < 8) { std::fprintf(stderr, "usage: xt_env rom.2.bin A D S R offTick ticks [curve] | xt_env rom.2.bin conv\n"); return 2; }
    if (!xtEnvLoadTables(argv[1])) { std::fprintf(stderr, "cannot read the tables\n"); return 1; }
    const int off = std::atoi(argv[6]), ticks = std::atoi(argv[7]);
    const XtEnvParams prm = xtEnvAdsr(std::atoi(argv[2]), std::atoi(argv[3]), std::atoi(argv[4]), std::atoi(argv[5]),
                                      argc > 8 ? std::atoi(argv[8]) : 2);
    Sh4 cpu;
    if (!cpu.loadRom(argv[1])) return 1;
    const uint32_t ram = 0x8C000000, env = ram, src = ram + 0x100, sp = ram + 0xF0000;
    cpu.mapRam(ram, 0x100000);
    uint32_t words[14] = {};
    words[0] = (uint32_t)prm.curve;
    std::memcpy(&words[1], &prm.scale, 4);
    words[3] = words[4] = 0x00124f80;                 // as the converter writes them (sync fields)
    std::memcpy(&words[5], prm.time, 16);
    std::memcpy(&words[9], prm.level, 20);
    cpu.write(src, words, sizeof words);
    auto run = [&](uint32_t fn, std::initializer_list<uint32_t> a) {
        if (!cpu.call(fn, a, sp)) { std::fprintf(stderr, "%08x: %s\n", fn, cpu.error().c_str()); std::exit(1); }
    };
    run(0x894efdb0, { env });
    run(0x894f0130, { env, src });
    XtEnvelope e;
    e.noteOn(prm);
    auto f32 = [&](uint32_t a) { uint32_t v = cpu.rd32(a); float x; std::memcpy(&x, &v, 4); return x; };
    int bad = 0;
    for (int t = 0; t <= ticks && bad < 5; ++t) {
        if (t == off) { run(0x894f03e0, { env }); e.noteOff(); }
        if (t > 0) { run(0x894efdf0, { env, 1 }); e.tick(1); }
        const int fst = (int)cpu.rd32(env), fph = (int)cpu.rd32(env + 0x44);
        const float fo = f32(env + 0x4c), fc = f32(env + 0x54), fon = f32(env + 0x58), fT = f32(env + 0x6c);
        const bool same = fst == e.stage() && fph == e.phase16() && fo == e.level() && fc == e.smooth()
                       && fon == e.onsetLevel() && fT == e.stageTime();
        if (!same || t % 50 == 0 || t == off)
            std::printf("%5d  fw st %d ph %5d out %.8g coef %.6g T %g on %g | port st %d ph %5d out %.8g coef %.6g T %g on %g%s\n",
                        t, fst, fph, fo, fc, fT, fon, e.stage(), e.phase16(), e.level(), e.smooth(), e.stageTime(),
                        e.onsetLevel(), same ? "" : "  <-- differs");
        bad += !same;
        if (fst == 5 && e.done()) { std::printf("%5d  both done\n", t); break; }
    }
    std::printf(bad ? "DIFFERENT\n" : "identical\n");
    return bad ? 3 : 0;
}
#endif
