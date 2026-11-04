// The V-Synth XT's effect chip: see XtFxChip.h.
#include "XtFxChip.h"
#include <algorithm>
#include <cstring>

namespace {
inline int32_t sext(int32_t x, int n) { return (int32_t)((uint32_t)x << (32 - n)) >> (32 - n); }
inline int shiftOf(unsigned code) { static const int8_t s[4] = { 7, 6, 5, 3 }; return s[code & 3]; }      // ×1, ×2, ×4, ×16, on a product over 128
inline int32_t sat24(int32_t v) { return v > 0x7fffff ? 0x7fffff : v < -0x800000 ? -0x800000 : v; }
constexpr unsigned kRun = 768, kDecoded = 756;                                                               // clocks a sample; from 756 on, words aren't decoded (tail)
} // namespace

XtFxChip::XtFxChip() { e_.m.assign(1u << 18, 0); reset(); }

void XtFxChip::reset() {
    std::memset(words_, 0, sizeof words_);
    for (auto& k : instr_) for (auto& i : k) i = Instr();
    std::memset(core_, 0, sizeof core_);
    for (auto& c : core_) c.jumpAt = c.jumpTo = -1;
    std::memset(gram_, 0, sizeof gram_); std::memset(coef_, 0, sizeof coef_); mailbox_ = 0; pos_ = 0;
    std::fill(e_.m.begin(), e_.m.end(), 0);
    e_.pos = e_.eff = e_.imm = 0; e_.readLatch = e_.writeLatch = e_.var = 0; e_.stage = -1; e_.toCommit = 0; e_.mode = e_.modeCur = 0;
    hostTail_[0] = hostTail_[1] = -1; std::memset(staging_, 0, sizeof staging_); mode_ = 0;
}

XtFxChip::Instr XtFxChip::decode(uint32_t w) {
    Instr d; d.field = w >> 23 & 31; const unsigned op = (w >> 16) & 0x7c; d.mem = w >> 10 & 255; const unsigned code = w >> 8 & 3; d.shift = (uint8_t)shiftOf(code); d.coef = (int8_t)(w & 255);
    bool replace = false;
    const bool constant = d.mem >= 1 && d.mem <= 4; d.constant = constant ? 0x10 << (6 * (d.mem - 1)) : 0;       // operands 1…4: 0x10, 0x400, 0x10000, 0x400000
    const Kind plain = constant ? MacConstant : Mac;
    if (!w) { d.kind = Nop; return d; }
    switch (op) {
        case 0x00: d.kind = plain; break;
        case 0x04: d.kind = plain; replace = true; break;
        case 0x10: d.kind = plain; d.toB = 1; break;
        case 0x14: d.kind = plain; d.toB = 1; replace = true; break;
        case 0x08: d.kind = StoreSat; replace = true; break;
        case 0x0c: d.kind = StoreSat; replace = true; d.fromB = 1; break;
        case 0x18: d.kind = StoreSat; replace = true; d.toB = 1; break;
        case 0x1c: d.kind = StoreSat; replace = true; d.toB = d.fromB = 1; break;
        case 0x58: d.kind = StoreSat; break;
        case 0x5c: d.kind = StoreSat; d.toB = d.fromB = 1; break;
        case 0x24: d.kind = GramRead; d.toB = code & 2 ? 1 : 0; d.shift = code & 1 ? 6 : 7; replace = true; break;
        case 0x30: d.kind = MulCoef; d.toB = d.coef & 2 ? 1 : 0; d.fromB = d.toB; replace = !(d.coef & 1); break;
        case 0x34:
            d.kind = plain;
            if (d.mem >= 0xa0 && d.mem < 0xb0) { d.kind = constant ? plain : SetCoef; d.coefIndex = d.mem >> 1 & 7; d.fromB = d.mem & 1; }
            if (d.mem >= 0xc0 || (d.mem & 0xcf) == 0x45) {                       // the control forms: which accumulator and whether it's replaced are in the operand
                d.toB = d.mem & 0x20 ? 1 : 0; d.fromB = d.toB; replace = d.mem & 0x10; const unsigned sub = d.mem & 15;
                if (sub == 2) { d.kind = Branch; d.target = (uint16_t)(w & 0x3ff); }
                else if (sub == 6) d.kind = Dmac;
                else if (sub == 7) d.kind = SetAddress;
                else if (sub == 0xb) d.kind = SetWriteData;
                else if (sub >= 0xc) { d.kind = ReadData; d.mem |= 0xf0; }       // and into r252…r255
            }
            break;
        case 0x38: case 0x3c: d.kind = GramStoreAdd; d.fromB = op == 0x3c; break;
        case 0x40: d.kind = StoreRaw; break;
        case 0x44: d.kind = StoreRaw; replace = true; break;
        case 0x48: d.kind = StoreRect; break;
        case 0x4c: d.kind = StoreRect; replace = true; break;
        case 0x50: d.kind = constant ? CompareConstant : Compare; replace = true; break;
        case 0x6c: d.kind = FoldPos; replace = true; break;
        case 0x78: d.kind = FoldNeg; break;
        case 0x7c: d.kind = FoldNeg; replace = true; break;
        default: d.kind = plain; break;                                          // 0x20, 0x28, 0x2c, 0x54, 0x60…0x68, 0x70, 0x74: nothing in rom.2 uses them, so nothing could check them
    }
    d.keep = replace ? 0 : -1;
    return d;
}

void XtFxChip::setWord(uint32_t index, uint32_t value) {
    const unsigned k = index >> 10 & 1, pc = index & 0x3ff;
    words_[k][pc] = value; instr_[k][pc] = decode(value);
}

void XtFxChip::hostWrite(uint32_t address, uint8_t value) {
    address &= 0x3fff;
    if (address == 0x2003) { mode_ = value & 3; return; }                                 // the driver only writes 0x34…0x37: conditional branches on, the memory in two phases of 12 bits
    if (address >= 0x2000) return;                                                        // 0x2001, 0x2002: nothing the sound depends on
    staging_[address & 3] = value;
    if ((address & 3) == 3) commit(address >> 2);
}

void XtFxChip::commit(uint32_t addr) {
    hostTail_[addr >> 10 & 1] = (int32_t)(addr & 0x3ff);
    const auto wordAt = [this](uint32_t i) { return words_[i >> 10 & 1][i & 0x3ff]; };
    if (mode_ == 3) { mailbox_ = (int32_t)wordAt(addr); return; }
    const unsigned n = mode_ == 0 ? 1 : mode_ == 1 ? 2 : 5;
    if (addr + n > 2048) return;
    uint32_t w[5] = {}; for (unsigned i = 0; i < n; i++) w[i] = wordAt(addr + i);
    const uint32_t b0 = staging_[0], b1 = staging_[1], b2 = staging_[2], b3 = staging_[3];
    if (mode_ == 0) w[0] = b0 | b1 << 8 | b2 << 16 | b3 << 24;
    else if (mode_ == 1) { w[0] = (w[0] & ~0x3ffu) | b0 | (b1 & 3) << 8; w[1] = (w[1] & ~0x3ffu) | b1 >> 2 | (b2 & 15) << 6; }
    else {
        const uint32_t f[5] = { b0 >> 3, b1 & 31, b1 >> 5 | (b2 & 3) << 3, b2 >> 2 & 31, b2 >> 7 | (b3 & 15) << 1 };
        for (unsigned i = 0; i < 5; i++) w[i] = (w[i] & ~(31u << 23)) | f[i] << 23;
    }
    for (unsigned i = 0; i < n; i++) setWord(addr + i, w[i]);
}

int32_t XtFxChip::coefValue(unsigned i, unsigned t) const {
    int32_t v = coef_[i];
    if (i == 6) v = (int32_t)(((uint32_t)e_.var << 11) & 0x7fffff);             // the address register's fraction
    if (t == 8) v = ~v;
    if (t == 16) v = v >= 0 ? (~v & 0x7fffff) : ~(v & 0x7fffff);
    return v;
}

// ------------------------------------------------------------------ the delay memory
// A word with [27:26] set starts an operation (1 read, 2 write, 3 read at the address register); the next four words' fields are its offset, low first.  It takes the position and
// the offset at +5 and transfers at +7.  JE-8086's chip does it in two phases of 12 bits, and can start one right behind another, losing the low phase of a write.  rom.2's programs always
// leave six words between operations and never look until a transfer is done, so here it is one step (the check says it's the same).
inline void XtFxChip::memoryClock(uint8_t f) {
    Eram& e = e_;
    if (e.stage < 0) {
        if (f & 0x18) { e.stage = 0; e.mode = f; e.imm = 0; }
    } else if (++e.stage <= 4) e.imm += (uint32_t)f << ((unsigned)(e.stage - 1) * 5);
    if (e.toCommit && --e.toCommit == 0) {
        if ((e.modeCur & 0x18) == 0x10) { e.m[e.eff] = e.writeLatch; e.readLatch = e.writeLatch; }
        else e.readLatch = e.m[e.eff];
    }
    if (e.stage == 5) {
        e.modeCur = e.mode; e.toCommit = 2; e.stage = -1;
        e.eff = ((e.mode & 0x18) == 0x18 ? e.pos + ((uint32_t)e.var >> 12) + ((e.imm >> 1) & 1) : e.pos + e.imm) & 0x3ffff;   // the address register's integer part; bit 1 of its read's offset is +1, the interpolation's second point
    }
}

// ------------------------------------------------------------------ the cores
// One core's whole sample.  The accumulators are 30 bits, wrapping; a store reads them as they were three clocks ago (hist, of which only the last three matter).  A compare is slow to
// arrive: A's sign, after the next word, reaches the multiplicand at +5, and not negative blanks it at +5, +6, negative at +7…+10.
template <unsigned K>
inline void XtFxChip::runCore() {
    Core& c = core_[K];
    const uint32_t pos = pos_;
    int32_t* const iram = c.iram;
    int32_t acc[2] = { c.acc[0], c.acc[1] }, hist[2][4];
    std::memcpy(hist, c.hist, sizeof hist);
    uint32_t h = c.h, pc = 0;
    int32_t lastIn = c.lastIn, jumpAt = c.jumpAt, jumpTo = c.jumpTo;
    bool lastMul = c.lastMul;
    uint32_t cond = (uint32_t)c.skipPos | c.skipNeg | c.ready | (c.pending ? 1u : 0u);
    for (unsigned cyc = 0; cyc < kDecoded; cyc++) {
        const Instr* dp;
        if constexpr (K == 1) {
            if (pc >= kRun) { memoryClock(0); continue; }                        // a branch can run it off the end, where it stops until the next sample
            if ((int32_t)pc == jumpAt) { pc = (uint32_t)jumpTo; jumpAt = -1; }
            dp = &instr_[1][pc++];
            memoryClock(dp->field);
        } else dp = &instr_[0][cyc];
        const Instr& d = *dp;
        bool fresh = false;
        if (d.kind != Nop) {
            int32_t x, y = d.coef;
            const uint32_t at = (pos + d.mem) & 255;
            const uint32_t old = (h + 1) & 3;
            const int32_t stored = hist[d.fromB][old];
            switch (d.kind) {
                case Nop: x = 0; break;
                case Mac: x = iram[at]; break;
                case MacConstant: x = d.constant; break;
                case Dmac: x = lastIn >> 7; if (lastMul) y = (coefValue((uint8_t)d.coef >> 5, c.lastTransform) >> 9) & 0x7f; break;    // the low half of a 16-bit multiply
                case StoreSat: iram[at] = x = sat24(stored); break;
                case GramRead: x = gram_[at]; break;
                case GramStoreAdd: gram_[at] = sat24(stored); x = iram[at]; break;
                case SetCoef: coef_[d.coefIndex] = sat24(stored); x = iram[at]; break;
                case MulCoef: {
                    const uint8_t fl = (uint8_t)d.coef;
                    x = d.constant ? d.constant : iram[at];
                    if (fl & 4) { x = sat24(stored); if ((fl & 0x1c) == 0x1c) x = x >= 0 ? 0x7fffff : -0x800000; iram[at] = x; }
                    c.lastTransform = fl & 0x18;
                    y = coefValue(fl >> 5, c.lastTransform) >> 16;
                    break; }
                case Branch: x = iram[at]; if (stored >= 0) { jumpTo = d.target; jumpAt = (int32_t)pc + 2; } break;
                case SetAddress: e_.var = stored; x = iram[at]; break;
                case SetWriteData: e_.writeLatch = sat24(stored); x = iram[at]; break;
                case ReadData: x = e_.readLatch; iram[at] = x; break;
                case StoreRaw: iram[at] = x = sext(hist[0][old], 24); break;
                case StoreRect: iram[at] = x = std::max(0, sat24(hist[0][old])); break;
                case Compare: x = iram[at]; fresh = true; break;
                case CompareConstant: x = d.constant; fresh = true; break;
                case FoldPos: iram[at] = x = sat24(hist[0][old]); if (x >= 0) x = ~x; x &= 0x7fffff; break;
                case FoldNeg: iram[at] = x = sat24(hist[0][old]); if (x < 0) x = ~x; x &= 0x7fffff; break;
            }
            if (cond && ((c.negative ? c.skipNeg : c.skipPos) & 1)) x = 0;
            acc[d.toB] = sext((acc[d.toB] & d.keep) + ((x * y) >> d.shift), 30);
            lastIn = x; lastMul = d.kind == MulCoef;
        }
        if (cond | fresh) {
            c.skipPos >>= 1; c.skipNeg >>= 1;
            if (c.ready & 1) c.negative = c.values & 1;
            c.ready >>= 1; c.values >>= 1;
            if (c.pending) { c.ready |= 4; if (acc[0] < 0) c.values |= 4; c.skipPos |= 0x18; c.skipNeg |= 0x1e0; }
            c.pending = fresh;
            cond = (uint32_t)c.skipPos | c.skipNeg | c.ready | (c.pending ? 1u : 0u);
        }
        hist[0][h & 3] = acc[0]; hist[1][h & 3] = acc[1]; h = (h + 1) & 3;
    }
    if constexpr (K == 1) {                                                      // the last twelve clocks: words multiply into A undecoded, by what the raw registers hold and then by the words
        for (unsigned t = 0; t < kRun - kDecoded; t++) {                         // the host last touched.  No store reads what it leaves, but the state is the chip's, and it costs little.
            if (pc < kRun) {                                                     // (On core 0 it comes to nothing: r0 and r255 stay 0, and the words before it are empty.)
                pc++;
                if (t < 2) lastIn = sext(iram[(pos + (t ? 0xffu : 0u)) & 255], 24);
                else if (hostTail_[1] >= 0) {
                    const uint32_t a = (uint32_t)hostTail_[1] + (t - 2) / 2;
                    const uint32_t w = words_[1][hostTail_[1] < (int32_t)kRun ? std::min(a, kRun - 1) : a & 0x3ff];
                    acc[0] = sext(acc[0] + (int32_t)(((int64_t)lastIn * sext((int32_t)(w & 255), 8)) >> shiftOf(w >> 8 & 3)), 30);
                }
            }
            hist[0][h & 3] = acc[0]; hist[1][h & 3] = acc[1]; h = (h + 1) & 3;
        }
    }
    c.acc[0] = acc[0]; c.acc[1] = acc[1]; std::memcpy(c.hist, hist, sizeof hist); c.h = h;
    c.lastIn = lastIn; c.jumpAt = jumpAt; c.jumpTo = jumpTo; c.lastMul = lastMul;
}

// A frame is each core in turn: they share GRAM and the coefficient registers, but rom.2's programs read neither in the sample the other core writes it, and never both want GRAM at once.
void XtFxChip::runFrame() {
    runCore<1>();                                                                // core 1 first, as it alone drives the memory
    runCore<0>();
    pos_ = (pos_ - 1) & 255;
    e_.pos = (e_.pos - 1) & 0x3ffff;
}

// ------------------------------------------------------------------ the check
#ifdef XT_FX_MAIN
// xt_fx <rom> [frames] [seed]: this class against JE-8086's interpreter, unmodified, on the XT's own programs loaded by the firmware's own driver in the interpreter: each MFX, Reverb
//   and Chorus type with its parameters drawn at random and changed as it runs, noise at every level into every input, and then all of them changing type under load.  Every word of
//   both cores' state and all of GRAM, each sample, and the delay memory each 2048 samples.  XT_FX_FIRST=1: say where each case first differs.  XT_FX_STOP=1: stop at the first case that does.
// xt_fx <rom> numbers: the firmware's own numbers on this class: gains, the EQ, the delays, whether every type makes sense.
// xt_fx <rom> speed: how much of a core it takes.
// Build: see research/tools/build.sh.
#include "Sh4Interp.h"
#include "esp_interpreter.h"
#include <cmath>
#include <ctime>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <memory>
#include <string>

namespace {
using Reference = esp::Interpreter<18>;
constexpr uint32_t kSp = 0x8C0E0000, kMfxObj = 0x89cef260, kMfxIds = 0x897a8218, kRevIds = 0x897a8310, kChoIds = 0x897a82e4, kDesc = 0x897a56d8;
const char* kRevName[] = { "Off", "Room 1", "Room 2", "Room 3", "Hall 1", "Hall 2", "Hall 3", "Garage", "Plate", "Non-Linear", "Delay" };
const char* kChoName[] = { "Off", "Chorus 1", "Chorus 2", "Chorus 3", "Chorus 4", "Feedback Chorus", "Flanger", "Short Delay", "Fbk Short Delay" };

struct Rng { uint32_t s; explicit Rng(uint32_t seed) : s(seed * 2654435761u + 1) {} uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; } int below(int n) { return (int)(next() % (uint32_t)n); } };

// The firmware's driver, with the chip's bus at its four primitives.  Each sends what the routine sends, byte for byte (read off 0x894af400, 0x894af510, 0x894af610, 0x894af6a0).
struct Rig {
    Sh4 c; XtFxChip chip; std::unique_ptr<Reference> ref; uint32_t obj = 0, vt = 0; int mfx = -1; uint64_t readsDiffer = 0;
    explicit Rig(const char* rom, bool withReference = true) {
        if (withReference) ref = std::make_unique<Reference>();
        if (!c.loadRom(rom)) { std::printf("cannot load %s\n", rom); std::exit(1); }
        c.mapRam(0x8C000000, 0x02000000);
        c.hook(0x894af3a0, [](Sh4& m) { m.ret(0); }); c.hook(0x894af3c0, [](Sh4& m) { m.ret(0); }); c.hook(0x894b26a0, [](Sh4& m) { m.ret(0); });
        c.hook(0x894af400, [this](Sh4& m) { put(0x34, (int16_t)m.r[4], m.r[5]); m.ret(0); });
        c.hook(0x894af510, [this](Sh4& m) { m.ret(get((int16_t)m.r[4])); });
        c.hook(0x894af610, [this](Sh4& m) { const uint32_t v = (uint16_t)m.r[5], s = m.r[6] & 0xff;
            put(0x35, (int16_t)m.r[4], (v >> 8 & 0xff) | ((v << 1 & 0xfc) | (s & 3)) << 8 | ((v >> 7 & 1) | (s << 2 & 12)) << 16); m.ret(0); });
        c.hook(0x894af6a0, [this](Sh4& m) { const int i = (int16_t)m.r[4]; const uint32_t v = m.r[5], a = get(i), b = get(i + 4);
            put(0x36, i, (a >> 20 & 0xf8) | (v & 0xff) << 8 | (v >> 8 & 0xff) << 16 | ((v >> 16 & 3) | (b >> 24 & 0xc0)) << 24); m.ret(0); });
    }
    void bus(uint32_t a, uint8_t v) { chip.hostWrite(a, v); if (ref) ref->hostWrite(a, v); }
    void put(uint8_t cmd, int i, uint32_t w) { bus(0x2003, cmd); const uint32_t a = (uint32_t)i * 4; for (uint32_t k = 0; k < 4; k++) bus(a + k, (uint8_t)(w >> (8 * k))); }
    uint32_t get(int i) {
        put(0x37, i, 0);
        uint32_t v = 0; for (uint32_t k = 0; k < 4; k++) v |= (uint32_t)chip.hostRead(k) << (8 * k);
        if (ref) { uint32_t u = 0; for (uint32_t k = 0; k < 4; k++) u |= (uint32_t)ref->hostRead(k) << (8 * k); if (u != v) readsDiffer++; }
        return v & 0x0fffffff;
    }
    uint32_t call(uint32_t fn, std::initializer_list<uint32_t> a = {}) { uint32_t r = 0; if (!c.call(fn, a, kSp, &r)) { std::printf("fault in %08x: %s\n", fn, c.error().c_str()); std::exit(1); } return r; }
    int s16(uint32_t a) { return (int16_t)c.rd16(a); }
    int pick(int id, Rng* r) {                                                  // a parameter: its default, or anything in range
        const uint32_t d = kDesc + 8 * (uint32_t)(id - 0x55d); const int def = s16(d + 2), lo = s16(d + 4), hi = s16(d + 6);
        return r && r->below(2) ? lo + r->below(hi - lo + 1) : def;
    }
    uint32_t vcall(uint32_t off) { const uint32_t d = c.rd32(vt + off), fn = c.rd32(vt + off + 8); return call(fn, { obj + d }); }
    void setMfx(int t, Rng* r, const char* over = nullptr) {
        if (mfx >= 0) vcall(0x84);
        obj = call(0x894b43a0, { kMfxObj, (uint32_t)t }); vt = c.rd32(obj + 0x54); c.wr32(obj + 0x58, (uint32_t)t); c.wr32(obj + 0xdc, 12000); mfx = t;
        vcall(0x6c);
        mfxParams(r, over);
    }
    void mfxParams(Rng* r, const char* over = nullptr) {
        const int first = s16(kMfxIds + 4 * (uint32_t)mfx), last = s16(kMfxIds + 4 * (uint32_t)mfx + 2), n = mfx ? last - first + 1 : 0;
        for (int i = 0; i < n && i < 32; i++) c.wr16(obj + 0x5c + 4 * (uint32_t)i, (uint16_t)pick(first + i, r));
        if (over) { const std::string q = over; size_t at = 0; for (;;) { const int i = std::atoi(q.c_str() + at); const size_t e = q.find('=', at); c.wr16(obj + 0x5c + 4 * (uint32_t)i, (uint16_t)std::atoi(q.c_str() + e + 1)); const size_t cm = q.find(',', e); if (cm == std::string::npos) break; at = cm + 1; } }
        for (int k = 0; k < 100 && !vcall(0x78); k++) {}
    }
    void setReverb(int t, Rng* r) {
        call(0x894d0a70, { (uint32_t)t });
        const int first = s16(kRevIds + 4 * (uint32_t)t), last = s16(kRevIds + 4 * (uint32_t)t + 2);
        for (int i = 0; i < 24; i++) call(0x894d0a80, { (uint32_t)i, (uint32_t)(i <= last - first ? pick(first + i, r) : 0) });
        call(0x894d0b00, { 0 });
    }
    void setChorus(int t, Rng* r) {
        call(0x894d3420, { (uint32_t)t });
        const int first = s16(kChoIds + 4 * (uint32_t)t);
        for (int i = 0; i < 10; i++) call(0x894d3430, { (uint32_t)i, (uint32_t)pick(first + i, r) });
        call(0x894d3470, { 0 });
    }
    void levels(Rng* r) {                                                       // what a patch and the system set: Master, Output Gain, the MFX's and returns' levels and sends
        const auto v = [&](uint32_t def) { return r ? (uint32_t)r->below(0x8000) : def; };
        call(0x894af1b0, { r ? (uint32_t)r->below(128) : 127u }); call(0x894af2d0, { r ? (uint32_t)r->below(25) - 12 : 0u });
        call(0x894afdf0, { v(0x7fff) }); call(0x894afef0, { v(0) }); call(0x894aff00, { v(0) }); call(0x894aff10, { v(0) }); call(0x894aff20, { v(0x7fff) }); call(0x894aff50, { v(0x7fff) });
        call(0x894afd50, { r ? (uint32_t)r->below(2) : 0u });
    }
    void boot(int mfxType, int revType, int choType, Rng* r, const char* over = nullptr) {
        call(0x894afc50);
        call(0x894af1b0, { 127 }); call(0x894af2d0, { 0 }); call(0x894b0020, { 2 }); call(0x894ae960); call(0x894ae9a0);
        levels(nullptr);
        call(0x894d09b0); setReverb(revType, r);
        call(0x894d3350); setChorus(choType, r);
        setMfx(mfxType, r, over);
        call(0x894aff80, { 1 }); call(0x894affa0, { 0 }); call(0x894affc0, { 1 }); call(0x894affe0, { 1 });
        call(0x894b3020); call(0x894b3070);
        call(0x894af610, { 0x27, 0x20, 0 }); call(0x894af610, { 0x34, 0x20, 0 }); call(0x894aee10, { 1 });
    }
    void input(unsigned m, int32_t v) { chip.setGram(m, v); if (ref) ref->runtime().writeGram(v, (uint8_t)m); }
    void frame() { chip.runFrame(); if (ref) { ref->runProgram(768); ref->sync(); } }
};

// Every word of the two states, after the rotation.  Returns the first that differs.
std::string differs(const Rig& g, bool memory) {
    const auto& r = g.ref->runtime(); const XtFxChip& x = g.chip; char b[120];
    if (x.position() != r.core[0].iramPos) return "position";
    for (unsigned i = 0; i < 256; i++) if (x.gram(i) != r.readGram(i)) { std::snprintf(b, sizeof b, "GRAM %02x: %d, against %d", i, x.gram(i), r.readGram(i)); return b; }
    for (unsigned k = 0; k < 2; k++) {
        const auto& c = r.core[k];
        for (unsigned i = 0; i < 256; i++) if (x.iram(k, i) != c.readIram(i)) { std::snprintf(b, sizeof b, "IRAM %u %02x: %d, against %d", k, i, x.iram(k, i), c.readIram(i)); return b; }
        if (x.accA(k) != c.accA.acc || x.accB(k) != c.accB.acc) { std::snprintf(b, sizeof b, "core %u accumulators: %d %d, against %d %d", k, x.accA(k), x.accB(k), c.accA.acc, c.accB.acc); return b; }
        for (unsigned age = 1; age <= 3; age++) {                                // [head] is the oldest, three clocks back
            const unsigned i = ((unsigned)c.accA.head + 3 - age) % 3;
            if (x.historyA(k, age) != c.accA.hist[i] || x.historyB(k, age) != c.accB.hist[i]) { std::snprintf(b, sizeof b, "core %u pipeline, %u clocks back", k, age); return b; }
        }
    }
    for (unsigned i = 0; i < 8; i++) if (x.coefficient(i) != r.mulcoeffs[i]) { std::snprintf(b, sizeof b, "coefficient %u: %d, against %d", i, x.coefficient(i), r.mulcoeffs[i]); return b; }
    if (x.readLatch() != r.eram.readLatch) { std::snprintf(b, sizeof b, "read latch: %d, against %d", x.readLatch(), r.eram.readLatch); return b; }
    if (x.writeLatch() != r.eram.writeLatch) return "write latch";
    if (x.varOffset() != r.eram.varOffset) return "address register";
    if (((x.memoryPosition() ^ r.eram.pos) & 0x3ffff) != 0) return "memory's position";
    if (memory) for (uint32_t a = 0; a < (1u << 18); a++) if (x.memoryWord(a) != r.eram.readRaw(a)) { std::snprintf(b, sizeof b, "memory %05x: %d, against %d", a, x.memoryWord(a), r.eram.readRaw(a)); return b; }
    return "";
}

// A run: noise into every input at levels that change, the parameters changing.  Prints a line.
void compare(const char* rom, const char* label, int mfxType, int revType, int choType, int frames, uint32_t seed, bool roam) {
    Rig g(rom); Rng r(seed);
    g.boot(mfxType, revType, choType, &r); g.levels(&r);
    static const unsigned inputs[] = { 0x20, 0x22, 0x24, 0x26, 0x28, 0x2a, 0x2c, 0x2e, 0x18, 0x1a };
    double level[10] = {}; int32_t noise = 0;
    int different = 0, firstAt = -1; std::string what;
    const bool first = std::getenv("XT_FX_FIRST");
    for (int n = 0; n < frames; n++) {
        if (n % 256 == 0) { const int w = r.below(10); const int k = r.below(4); level[w] = k == 0 ? 0.0 : k == 1 ? 0.002 : k == 2 ? 0.25 : 2.0; }     // silence, low, mid, and enough to saturate
        if (n % 1024 == 512) { const int k = r.below(4);
            if (k == 0) g.mfxParams(&r); else if (k == 1) g.setReverb(revType, &r); else if (k == 2) g.setChorus(choType, &r); else g.levels(&r); }
        if (roam && n % 2048 == 1024) { const int k = r.below(3);
            if (k == 0) g.setMfx(r.below(42), &r); else if (k == 1) { revType = r.below(11); g.setReverb(revType, &r); } else { choType = r.below(9); g.setChorus(choType, &r); } }
        for (int i = 0; i < 10; i++) { noise = (int32_t)r.next(); const double v = level[i] * (noise / 2147483648.0) * 8388608.0; g.input(inputs[i], v > 8388607.0 ? 8388607 : v < -8388608.0 ? -8388608 : (int32_t)v); }
        g.frame();
        const std::string d = differs(g, n % 2048 == 2047 || n == frames - 1);
        if (!d.empty()) { if (!different) { firstAt = n; what = d; } different++; }
    }
    if (different && first) std::printf("  %s: first at sample %d, %s\n", label, firstAt, what.c_str());
    std::printf("%s: %d samples, %d different%s\n", label, frames, different + (g.readsDiffer ? 1 : 0), g.readsDiffer ? ", and reads back" : "");
    if ((different || g.readsDiffer) && std::getenv("XT_FX_STOP")) { std::fflush(stdout); std::exit(0); }
}

// ------------------------------------------------------------------ the firmware's own numbers
struct Tone { double in = 0, peak[2] = {}; };
Tone sine(Rig& g, unsigned slot, double f, double amp, int n, unsigned outL = 0xa0) {
    Tone t;
    for (int i = 0; i < n; i++) {
        const int32_t v = (int32_t)std::lrint(amp * std::sin(2 * M_PI * f * i / 44100.0) * 8388608.0);
        for (unsigned s = 0x20; s <= 0x2e; s += 2) g.chip.setGram(s, s == slot || s == slot + 2 ? v : 0);
        const uint32_t p = g.chip.position();
        g.chip.runFrame();
        if (i > n / 2) { t.in = std::max(t.in, std::fabs((double)v)); for (unsigned k = 0; k < 2; k++) t.peak[k] = std::max(t.peak[k], std::fabs((double)g.chip.gram(outL + 2 * k - (g.chip.position() - p)))); }
    }
    return t;
}
double db(double x) { return x > 0 ? 20 * std::log10(x) : -999.0; }
double gain(const char* rom, int mfx, unsigned slot, double f, double amp, const char* over = nullptr, unsigned out = 0xa0) {
    Rig g(rom, false); g.boot(mfx, 0, 0, nullptr, over);
    const Tone t = sine(g, slot, f, amp, 20000, out);
    return db(t.peak[0] / t.in);
}
// Where a click comes out: the first arrival, then the largest peaks after it, from the dry.
std::string echoes(const char* rom, int mfx, int samples, int ch) {
    Rig g(rom, false); g.boot(mfx, 0, 0, nullptr);
    std::vector<double> y;
    const int t0 = 100000;                                                       // past the delay's glide
    for (int i = 0; i < t0 + samples; i++) {
        const int32_t v = i == t0 ? 2097152 : 0;
        for (unsigned s = 0x20; s <= 0x2e; s += 2) g.chip.setGram(s, s == 0x28 || s == 0x2a ? v : 0);
        g.chip.runFrame();
        if (i >= t0) y.push_back(std::fabs((double)g.chip.gram(0xa1 + 2 * (unsigned)ch)));
    }
    int dry = 0; while (dry < (int)y.size() && y[(size_t)dry] < 100000.0) dry++;
    std::string s = "dry +" + std::to_string(dry);
    double top = 0; for (size_t i = (size_t)dry + 1000; i < y.size(); i++) top = std::max(top, y[i]);
    int shown = 0; for (size_t i = (size_t)dry + 1000; i < y.size() && shown < 3; i++) if (y[i] > 0.1 * top) { s += ", echo +" + std::to_string((int)i - dry); shown++; i += 500; }
    return s;
}
void numbers(const char* rom) {
    std::printf("MAIN to the mix: %+.2f dB\n", gain(rom, 0, 0x20, 1000, 0.25));
    std::printf("MULTI through Thru: %+.2f dB\n", gain(rom, 0, 0x28, 1000, 0.25));
    std::printf("the sends: into the Reverb, slot 72, %+.2f dB; into the Chorus, slot 70, through its filter at Off's values, %+.2f dB\n", gain(rom, 0, 0x24, 1000, 0.25, nullptr, 0x72), gain(rom, 0, 0x24, 1000, 0.25, nullptr, 0x70));
    const double flat = gain(rom, 1, 0x28, 800, 0.06);
    std::printf("Parametric EQ at 800 Hz, against flat: Mid 1 +15 %+.2f, +7 %+.2f, -7 %+.2f, -15 %+.2f; Total +15 %+.2f dB\n", gain(rom, 1, 0x28, 800, 0.06, "4=30") - flat, gain(rom, 1, 0x28, 800, 0.06, "4=22") - flat,
                gain(rom, 1, 0x28, 800, 0.06, "4=8") - flat, gain(rom, 1, 0x28, 800, 0.06, "4=0") - flat, gain(rom, 1, 0x28, 800, 0.06, "10=30") - flat);
    std::printf("Stereo Delay: L %s; R %s\n", echoes(rom, 11, 60000, 0).c_str(), echoes(rom, 11, 60000, 1).c_str());
    std::printf("Multi Tap: L %s; R %s\n", echoes(rom, 12, 50000, 0).c_str(), echoes(rom, 12, 50000, 1).c_str());
    int silent = 0, away = 0;
    for (int t = 0; t < 42; t++) { const double v = gain(rom, t, 0x28, 1000, 0.03); if (v < -40) silent++; if (v > 30) away++; }
    std::printf("MFX types at 1 kHz: of 42, %d silent, %d running away\n", silent, away);
    for (int rv = 1; rv <= 10; rv++) {                                          // the signal in a Reverb's tail, DC aside: a click into its send, 1 s and 3 s on
        Rig g(rom, false); g.boot(0, rv, 0, nullptr);
        double e1 = 0, e3 = 0, last = 0;
        for (int i = 0; i < 44100 * 3 + 4410; i++) {
            for (unsigned s = 0x20; s <= 0x2e; s += 2) g.chip.setGram(s, s == 0x24 && i == 100 ? 2097152 : 0);
            g.chip.runFrame();
            const double v = (double)g.chip.gram(0xa1), d = v - last; last = v;
            if (i >= 44100 && i < 44100 + 4410) e1 += d * d;
            if (i >= 44100 * 3) e3 += d * d;
        }
        std::printf("Reverb %-11s tail, DC aside: at 1 s %.1f dB, at 3 s %.1f dB\n", kRevName[rv], 10 * std::log10(e1 / 4410 + 1e-3) - 138.5, 10 * std::log10(e3 / 4410 + 1e-3) - 138.5);
    }
}
void speed(const char* rom) {
    for (const auto& c : { std::pair<int, int>(0, 0), std::pair<int, int>(1, 4), std::pair<int, int>(27, 4) }) {
        Rig g(rom, false); g.boot(c.first, c.second, 1, nullptr);
        Rng r(1); const std::clock_t t0 = std::clock(); const int n = 200000;                  // CPU time: the machine may be busy with something else
        for (int i = 0; i < n; i++) { for (unsigned s = 0x20; s <= 0x2e; s += 2) g.chip.setGram(s, (int32_t)(r.next() >> 12) - 524288); g.chip.runFrame(); }
        const double sec = (double)(std::clock() - t0) / CLOCKS_PER_SEC;
        std::printf("MFX %d, Reverb %d: %.2f x real time, %.0f%% of a core\n", c.first, c.second, (n / 44100.0) / sec, 100 * sec / (n / 44100.0));
    }
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 2) { std::fprintf(stderr, "xt_fx <rom> [frames] [seed] | numbers | speed\n"); return 2; }
    const char* rom = argv[1];
    if (argc > 2 && !std::strcmp(argv[2], "numbers")) { numbers(rom); return 0; }
    if (argc > 2 && !std::strcmp(argv[2], "speed")) { speed(rom); return 0; }
    const int frames = argc > 2 ? std::atoi(argv[2]) : 8192; const uint32_t seed = argc > 3 ? (uint32_t)std::atoi(argv[3]) : 2024;
    { Sh4 c; c.loadRom(rom); for (int t = 0; t < 42; t++) { char nm[24]; c.read(0x89788c98 + 24 * (uint32_t)t, nm, 20); nm[20] = 0; for (int e = 19; e > 0 && nm[e] == ' '; e--) nm[e] = 0; compare(rom, ("MFX " + std::to_string(t) + " " + nm).c_str(), t, t % 11, t % 9, frames, seed + (uint32_t)t, false); } }
    for (int t = 0; t < 11; t++) compare(rom, (std::string("Reverb ") + kRevName[t]).c_str(), 0, t, 0, frames, seed + 100 + (uint32_t)t, false);
    for (int t = 0; t < 9; t++) compare(rom, (std::string("Chorus ") + kChoName[t]).c_str(), 1, 0, t, frames, seed + 200 + (uint32_t)t, false);
    compare(rom, "every type, changing under load", 0, 0, 0, frames * 8, seed + 300, true);
    return 0;
}
#endif
