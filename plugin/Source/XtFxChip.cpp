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
    const unsigned n = wordsOf(mode_);
    if (addr + n > 2048) return;
    uint32_t w[5] = {}; for (unsigned i = 0; i < n; i++) w[i] = wordAt(addr + i);
    merge(mode_, staging_, w);
    for (unsigned i = 0; i < n; i++) setWord(addr + i, w[i]);
}

void XtFxChip::merge(unsigned mode, const uint8_t bytes[4], uint32_t w[5]) {
    const uint32_t b0 = bytes[0], b1 = bytes[1], b2 = bytes[2], b3 = bytes[3];
    if (mode == 0) w[0] = b0 | b1 << 8 | b2 << 16 | b3 << 24;
    else if (mode == 1) { w[0] = (w[0] & ~0x3ffu) | b0 | (b1 & 3) << 8; w[1] = (w[1] & ~0x3ffu) | b1 >> 2 | (b2 & 15) << 6; }
    else {
        const uint32_t f[5] = { b0 >> 3, b1 & 31, b1 >> 5 | (b2 & 3) << 3, b2 >> 2 & 31, b2 >> 7 | (b3 & 15) << 1 };
        for (unsigned i = 0; i < 5; i++) w[i] = (w[i] & ~(31u << 23)) | f[i] << 23;
    }
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
// xt_fx <rom> speed: how much of a core it takes, and what the control side costs.
// xt_fx <rom> control [events] [seed]: XtFxControl: see "the control side" below.  xt_fx <rom> threads [events] [seed]: only its part with threads.
// Build: see research/tools/build.sh.
#include "Sh4Interp.h"
#include "XtFxControl.h"
#include "esp_interpreter.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <ctime>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <initializer_list>
#include <map>
#include <memory>
#include <new>
#include <string>
#include <thread>

namespace {
using Reference = esp::Interpreter<18>;
constexpr uint32_t kSp = 0x8C0E0000, kMfxObj = 0x89cef260, kMfxIds = 0x897a8218, kRevIds = 0x897a8310, kChoIds = 0x897a82e4, kDesc = 0x897a56d8;
const char* kRevName[] = { "Off", "Room 1", "Room 2", "Room 3", "Hall 1", "Hall 2", "Hall 3", "Garage", "Plate", "Non-Linear", "Delay" };
const char* kChoName[] = { "Off", "Chorus 1", "Chorus 2", "Chorus 3", "Chorus 4", "Feedback Chorus", "Flanger", "Short Delay", "Fbk Short Delay" };

struct Rng { uint32_t s; explicit Rng(uint32_t seed) : s(seed * 2654435761u + 1) {} uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; } int below(int n) { return (int)(next() % (uint32_t)n); } };

// The firmware's driver, with the chip's bus at its four primitives.  Each sends what the routine sends, byte for byte (read off 0x894af400, 0x894af510, 0x894af610, 0x894af6a0).
struct Rig {
    Sh4 c; XtFxChip chip; std::unique_ptr<Reference> ref; uint32_t obj = 0, vt = 0; int mfx = -1; uint64_t readsDiffer = 0; const std::map<int, int>* forced = nullptr;
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
        if (forced) { const auto it = forced->find(id); if (it != forced->end()) return it->second; }
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
std::string differs(const XtFxChip& x, const Reference& ref, bool memory) {
    const auto& r = ref.runtime(); char b[120];
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
        const std::string d = differs(g.chip, *g.ref, n % 2048 == 2047 || n == frames - 1);
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
    // The control side.  The driver on its thread, and the most the audio thread ever gets between two samples: a Reverb's type, the longest program with the most read-backs.
    using Clock = std::chrono::steady_clock;
    const auto us = [](Clock::time_point a, Clock::time_point b) { return std::chrono::duration<double, std::micro>(b - a).count(); };
    const Clock::time_point t0 = Clock::now(); XtFxControl c(rom); const Clock::time_point t1 = Clock::now(); XtFxChip chip;
    while (!c.drained()) { c.service(chip); chip.runFrame(); }
    std::printf("the driver's start: %.0f us\n", us(t0, t1));
    const auto edit = [&](const char* what, int id, int v) {
        c.set(id, v); const Clock::time_point a = Clock::now(); c.update(); const Clock::time_point b = Clock::now(); double most = 0; long samples = 0;
        while (!c.drained()) { const Clock::time_point p = Clock::now(); c.service(chip); most = std::max(most, us(p, Clock::now())); chip.runFrame(); samples++; }
        std::printf("%s: %.0f us on the control thread; at most %.1f us between two samples; %.1f ms to reach the chip\n", what, us(a, b), most, samples / 44.1);
    };
    edit("an MFX level", 0x178, 100); edit("the MFX's type", 0x177, 27); edit("one of its parameters", 0x57d, 3); edit("the Reverb's type", 0x230, 4); edit("the Chorus's type", 0x1fc, 3);
}

// ------------------------------------------------------------------ the control side
// xt_fx <rom> control [events] [seed].  Each part says how many of what it tried were different.
// - Against the reference: the firmware's driver through XtFxControl with the bus straight on JE-8086's chip, whose waits run it, against the same driver through the queue into XtFxChip, whole
//   state each sample.  That's the queue, the copy of the words and the waits; the driver is the same code on both sides.
// - The readers: a patch through the firmware's readers, by ID from the store, against the routines the rest of this file calls one by one, with the same values, word for word.
// - Behaviour: what each kind of change costs on the bus, exactly; every ID a reader reads counts as a change; the Matrix reaches the types that assign it; clamping; no click under a tone.
// - Live edits: edits one at a time through update(), then the same store loaded as a patch into a new one.  The words must end up the same, but for what the firmware itself leaves behind.
// - Threads: the driver on one thread and the samples on another, as fast as they go, each stopping now and then: the stream arrives whole and in order, the waits are kept, and the audio thread
//   allocates nothing.  research/tools/fxthreads.sh runs this part under the thread sanitizer.
thread_local bool tAudioThread = false;
std::atomic<uint64_t> gAudioAllocations{0};

struct Edit { std::vector<std::pair<int, int>> sets; bool patch = false, matrix = false; int prm[3] = {}, clockMs = 0; };

// Draws edits, from the firmware's own ranges.
struct Editor {
    Sh4 rom; XtFxControl& ranges; Rng r; int mfx = 0, cho = 0, rev = 0;
    Editor(const char* path, XtFxControl& c, uint32_t seed) : ranges(c), r(seed) { rom.loadRom(path); }
    int s16(uint32_t a) { return (int16_t)rom.rd16(a); }
    int draw(int id) { const int lo = ranges.lowest(id), hi = ranges.highest(id); return lo + r.below(hi - lo + 1); }
    void params(Edit& e, uint32_t table, int type, int n) { const int first = s16(table + 4 * (uint32_t)type), last = s16(table + 4 * (uint32_t)type + 2); for (int i = 0; i < n; i++) { const int id = first + r.below(last - first + 1); e.sets.push_back({ id, draw(id) }); } }
    Edit mfxType(int t) { Edit e; mfx = t; e.sets.push_back({ 0x177, t }); params(e, kMfxIds, t, 3); return e; }
    Edit choType(int t) { Edit e; cho = t; e.sets.push_back({ 0x1fc, t }); params(e, kChoIds, t, 3); return e; }
    Edit revType(int t) { Edit e; rev = t; e.sets.push_back({ 0x230, t }); params(e, kRevIds, t, 3); return e; }
    Edit any() {
        Edit e; static const int levels[] = { 0x178, 0x179, 0x17a, 0x1fd, 0x1fe, 0x231, 0x3e, 0x76, 0x51e, 0x11d, 0x120, 0x121, 0x122, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70 };
        switch (r.below(10)) {
            case 9: e.matrix = true; for (int& v : e.prm) { const int k = r.below(4); v = k == 0 ? 0 : k == 1 ? r.below(128) - 64 : (int)(r.next() % 65535) - 32767; } break;
            case 0: return mfxType(r.below(42));
            case 1: return choType(r.below(9));
            case 2: return revType(r.below(11));
            case 3: case 4: params(e, kMfxIds, mfx, 1 + r.below(4)); break;
            case 5: params(e, kChoIds, cho, 1 + r.below(3)); break;
            case 6: params(e, kRevIds, rev, 1 + r.below(3)); break;
            case 7: { const int id = levels[r.below((int)(sizeof levels / sizeof *levels))]; e.sets.push_back({ id, draw(id) }); break; }
            default: e = mfxType(r.below(42)); e.patch = true; break;
        }
        return e;
    }
};
void apply(XtFxControl& c, const Edit& e) { if (e.matrix) { c.setMatrix(e.prm[0], e.prm[1], e.prm[2]); return; } if (e.clockMs) { c.outsideClock(e.clockMs); return; } for (const auto& s : e.sets) c.set(s.first, s.second); if (e.patch) c.loadPatch(); else c.update(); }

// The reference with the driver on its bus.  Its waits run the samples, and the samples run both chips.
struct DirectBus : XtFxControl::Bus {
    Reference& ref; std::function<void()> sample;
    explicit DirectBus(Reference& r) : ref(r) {}
    void write(uint8_t c, uint16_t i, uint32_t w) override { ref.hostWrite(0x2003, c); for (uint32_t k = 0; k < 4; k++) ref.hostWrite((uint32_t)i * 4 + k, (uint8_t)(w >> (8 * k))); }
    uint32_t read(uint16_t i) override { write(0x37, i, 0); uint32_t v = 0; for (uint32_t k = 0; k < 4; k++) v |= (uint32_t)ref.hostRead(k) << (8 * k); return v & 0x0fffffff; }
    void wait(uint32_t n) override { for (uint32_t i = 0; i < n; i++) sample(); }
};

void againstReference(const char* rom, int randomEvents, uint32_t seed) {
    auto ref = std::make_unique<Reference>(); auto chip = std::make_unique<XtFxChip>();
    Rng noise(seed); double level[10] = {}; long samples = 0, different = 0, firstAt = -1, words = 0; std::string what;
    static const unsigned inputs[] = { 0x20, 0x22, 0x24, 0x26, 0x28, 0x2a, 0x2c, 0x2e, 0x18, 0x1a };
    XtFxControl queued(rom);
    const auto sample = [&] {
        if (samples % 256 == 0) { const int w = noise.below(10), k = noise.below(4); level[w] = k == 0 ? 0.0 : k == 1 ? 0.002 : k == 2 ? 0.25 : 2.0; }
        for (int i = 0; i < 10; i++) { const double v = level[i] * ((int32_t)noise.next() / 2147483648.0) * 8388608.0; const int32_t x = v > 8388607.0 ? 8388607 : v < -8388608.0 ? -8388608 : (int32_t)v; chip->setGram(inputs[i], x); ref->runtime().writeGram(x, (uint8_t)inputs[i]); }
        queued.service(*chip); chip->runFrame(); ref->runProgram(768); ref->sync(); samples++;
        const std::string d = differs(*chip, *ref, samples % 2048 == 0);
        if (!d.empty()) { if (!different) { firstAt = samples; what = d; } different++; }
    };
    DirectBus bus(*ref); bus.sample = sample;
    XtFxControl direct(rom, &bus);                                               // starts both: its start's waits run the samples
    Editor ed(rom, queued, seed);
    const auto run = [&](const Edit& e, int after) {
        apply(queued, e); apply(direct, e);
        for (int i = 0; i < after; i++) sample();
        for (unsigned i = 0; i < 2048; i++) if (queued.drained() && (chip->word(i) & 0x0fffffff) != (queued.word(i) & 0x0fffffff)) { words++; break; }
    };
    for (int t = 1; t <= 42; t++) { run(ed.mfxType(t % 42), 128); Edit e; ed.params(e, kMfxIds, t % 42, 3); run(e, 128); }
    for (int t = 1; t <= 9; t++) { run(ed.choType(t % 9), 128); Edit e; ed.params(e, kChoIds, t % 9, 3); run(e, 128); }
    for (int t = 1; t <= 11; t++) { run(ed.revType(t % 11), 128); Edit e; ed.params(e, kRevIds, t % 11, 3); run(e, 128); }
    for (int i = 0; i < randomEvents; i++) {
        run(ed.any(), ed.r.below(4) ? ed.r.below(64) : ed.r.below(2048));
        if (i % 8 == 3) { Edit e; e.sets.push_back({ 0x41, ed.r.below(4) ? 1 : 0 }); run(e, 16); Edit c; c.clockMs = 60 + ed.r.below(400); run(c, 16); }   // an outside clock, on and off
    }
    if (different && std::getenv("XT_FX_FIRST")) std::printf("  control: first at sample %ld, %s\n", firstAt, what.c_str());
    if (!queued.ok() || !direct.ok()) std::printf("  control: a fault: %s %s\n", queued.error().c_str(), direct.error().c_str());
    std::printf("control against the reference: %ld samples, %ld different\n", samples, different + words + (queued.ok() && direct.ok() ? 0 : 1));
}

// A patch through the firmware's readers, by ID, against Rig's routines one by one.  Where they should differ, the Rig is told what it leaves out: the tempo, and that a Chorus of type Off keeps its
// return shut however its switch stands.  And the system's EQ, which the firmware starts with on, goes off.
void readers(const char* rom, uint32_t seed) {
    int cases = 0, different = 0; std::string first;
    for (int t = 0; t < 62; t++) {
        const int mfx = t < 42 ? t : 0, rev = t < 42 ? t % 11 : t < 53 ? t - 42 : 0, cho = t < 42 ? t % 9 : t < 53 ? 0 : t - 53;
        XtFxControl c(rom); Editor ed(rom, c, seed + (uint32_t)t); std::map<int, int> forced;
        const auto draw = [&](uint32_t table, int type) { const int f = ed.s16(table + 4 * (uint32_t)type), l = ed.s16(table + 4 * (uint32_t)type + 2); for (int id = f; id <= l; id++) { forced[id] = ed.draw(id); c.set(id, forced[id]); } };
        draw(kMfxIds, mfx); draw(kRevIds, rev); draw(kChoIds, cho);
        c.set(0x177, mfx); c.set(0x230, rev); c.set(0x1fc, cho); c.set(0x120, 1); c.set(0x121, 1); c.set(0x122, 1); c.set(0x70, 0); c.update(); c.loadPatch();
        Rig g(rom, false); g.forced = &forced;
        g.boot(mfx, rev, cho, nullptr);
        g.call(0x894d34e0, { 1200 }); g.call(0x894d0b50, { 1200 }); g.call(0x894affe0, { cho ? 1u : 0u });
        int d = 0; unsigned at = 0; uint32_t x = 0, y = 0;
        for (unsigned i = 0; i < 2048; i++) { const uint32_t a = g.chip.word(i) & 0x0fffffff, b = c.word(i) & 0x0fffffff; if (a != b && !d++) { at = i; x = a; y = b; } }
        if (d && first.empty()) { char s[120]; std::snprintf(s, sizeof s, "MFX %d, Reverb %d, Chorus %d: word %03x, %07x against %07x", mfx, rev, cho, at, y, x); first = s; }
        if (!c.ok()) { first = c.error(); d++; }
        cases++; different += d ? 1 : 0;
    }
    if (different && std::getenv("XT_FX_FIRST")) std::printf("  readers: first %s\n", first.c_str());
    std::printf("control's readers against the routines one by one: %d samples, %d different\n", cases, different);
}

// Edits one at a time through update(), then the same store as a patch into a new control.  Types have their defaults and the edits their own values, so anything set() lost shows.
void liveEdits(const char* rom, int events, uint32_t seed) {
    int cases = 0, different = 0; std::string first;
    for (int run = 0; run < 12; run++) {
        XtFxControl live(rom); Editor ed(rom, live, seed + 1000 + (uint32_t)run);
        for (int i = 0; i < events; i++) {
            Edit e = ed.any(); e.patch = false;
            if (e.matrix) continue;                                              // the Matrix isn't in the store, so a new patch wouldn't have it
            // Nor do these parameters reach the words alone, in the firmware's own update: a new patch has them and a live edit doesn't (HARDWARE_FIRMWARE.md, "XtFxControl").
            static const int alone[] = { 0x57b, 0x6bb, 0x6bd, 0x5b7, 0x5dd, 0x634, 0x71e };
            e.sets.erase(std::remove_if(e.sets.begin(), e.sets.end(), [](const std::pair<int, int>& x) { return std::find(std::begin(alone), std::end(alone), x.first) != std::end(alone); }), e.sets.end());
            apply(live, e);
        }
        XtFxControl fresh(rom);
        for (int id = 0; id < 0x8d0; id++) fresh.set(id, live.get(id));
        fresh.loadPatch();
        // The firmware leaves two things behind.  Reverb types Off and Delay don't write the pre-EQ (A 0x138…0x156), so it's what the last type left, and a new patch hasn't had one.  So through Reverb 1
        // on both, and back.  And Reverb Delay's note values use the tempo it last heard, which it only hears as the current type.  So the tempo away and back, on both.
        const int rev = live.get(0x230), tempo = live.get(0x11d);
        for (XtFxControl* c : { &live, &fresh }) { c->set(0x230, rev == 1 ? 2 : 1); c->update(); c->set(0x230, 1); c->update(); c->set(0x230, rev); c->update(); c->set(0x11d, tempo == 2500 ? tempo - 1 : tempo + 1); c->update(); c->set(0x11d, tempo); c->update(); }
        int d = 0; unsigned at = 0; uint32_t x = 0, y = 0;
        for (unsigned i = 0; i < 2048; i++) { const uint32_t a = live.word(i) & 0x0fffffff, b = fresh.word(i) & 0x0fffffff; if (a != b && !d++) { at = i; x = a; y = b; } }
        if (d && first.empty()) { char s[100]; std::snprintf(s, sizeof s, "run %d: %d words, first %03x, live %07x, patch %07x", run, d, at, x, y); first = s; }
        if (!live.ok() || !fresh.ok()) { first = live.error() + fresh.error(); d = 1; }
        if (d && std::getenv("XT_FX_LIVE")) { std::printf("  run %d, MFX %d, Chorus %d, Reverb %d:", run, live.get(0x177), live.get(0x1fc), live.get(0x230)); for (unsigned i = 0; i < 2048; i++) { const uint32_t a = live.word(i) & 0x0fffffff, b = fresh.word(i) & 0x0fffffff; if (a != b) std::printf(" %c%03x %07x/%07x", "AB"[i >> 10], i & 1023, a, b); } std::printf("\n"); }
        cases++; different += d ? 1 : 0;
    }
    if (different && std::getenv("XT_FX_FIRST")) std::printf("  live edits: %s\n", first.c_str());
    std::printf("control's live edits against a patch: %d samples, %d different\n", cases, different);
}

// The stream as it should be, one thread and no queue: a checksum of what's sent, and where each wait falls in it.
struct Recorder : XtFxControl::Bus {
    XtFxChip chip; uint32_t crc = 0; uint64_t n = 0; std::vector<std::pair<uint64_t, uint32_t>> waits;
    static uint32_t mix(uint32_t h, uint32_t v) { return (h ^ v) * 16777619u; }
    void write(uint8_t c, uint16_t i, uint32_t w) override { chip.hostWord(c, i, w); crc = mix(mix(mix(crc, c), i), w); n++; }
    uint32_t read(uint16_t i) override { write(0x37, i, 0); uint32_t v = 0; for (uint32_t k = 0; k < 4; k++) v |= (uint32_t)chip.hostRead(k) << (8 * k); return v & 0x0fffffff; }
    void wait(uint32_t s) override { waits.push_back({ n, s }); }
};
// What the audio thread sees.  No allocation here: what it records has its room already.
struct Tap {
    std::vector<uint32_t>& at; uint32_t crc = 0; uint64_t n = 0; uint32_t sample = 0; bool overflow = false;
    static void observe(void* context, uint8_t c, uint16_t i, uint32_t w) {
        Tap& t = *static_cast<Tap*>(context); t.crc = Recorder::mix(Recorder::mix(Recorder::mix(t.crc, c), i), w);
        if (t.n < t.at.size()) t.at[t.n] = t.sample; else t.overflow = true;
        t.n++;
    }
};

void threads(const char* rom, int events, uint32_t seed) {
    // A burst too big for the ring while the audio thread's stopped, then the rest with both going.
    std::vector<Edit> edits;
    { XtFxControl c(rom); Editor ed(rom, c, seed + 7); for (int i = 0; i < events; i++) edits.push_back(ed.any()); for (int i = 0; i < 3000; i++) { Edit e; const int id = 0x178 + (i & 1); e.sets.push_back({ id, i % 128 }); edits.push_back(e); } }
    // The order it'll go in, the burst in the middle, recorded with one thread and no queue.
    const size_t random = (size_t)events;
    Recorder expect; { XtFxControl e(rom, &expect);
        for (size_t i = 0; i < edits.size(); i++) { if (i == random / 2) for (size_t k = 0; k < 3000; k++) apply(e, edits[random + k]); if (i >= random && i < random + 3000) continue; apply(e, edits[i]); } }
    auto chip = std::make_unique<XtFxChip>(); std::vector<uint32_t> at((size_t)expect.n + 1024);
    Tap tap{ at };
    XtFxControl c(rom); c.observe(Tap::observe, &tap);
    std::atomic<bool> finished{false}; std::atomic<int> hold{0};                 // hold: 1, the audio thread waits; 2, it's waiting
    std::thread control([&] {
        Rng r(seed + 11);
        for (size_t i = 0; i < edits.size(); i++) {
            if (i == random / 2) { hold = 1; while (hold.load() != 2) std::this_thread::yield(); for (size_t k = 0; k < 3000; k++) apply(c, edits[random + k]); hold = 0; }
            if (i >= random && i < random + 3000) continue;
            apply(c, edits[i]);
            if (r.below(8) == 0) std::this_thread::sleep_for(std::chrono::microseconds(r.below(2000)));
        }
        while (!c.drained()) { c.pump(); std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
        finished = true;
    });
    tAudioThread = true;
    Rng r(seed + 13); uint64_t samples = 0;
    while (!finished.load()) {
        if (hold.load() == 1) { hold = 2; while (hold.load() == 2) std::this_thread::yield(); }
        c.service(*chip); chip->runFrame(); tap.sample++; samples++;
        if (r.below(4096) == 0) std::this_thread::sleep_for(std::chrono::microseconds(r.below(2000)));
    }
    tAudioThread = false;
    control.join();
    long short_ = 0; for (const auto& w : expect.waits) if (w.first > 0 && w.first < tap.n && w.first < at.size() && at[(size_t)w.first] - at[(size_t)w.first - 1] < w.second) short_++;
    long words = 0; for (unsigned i = 0; i < 2048; i++) if ((chip->word(i) & 0x0fffffff) != (c.word(i) & 0x0fffffff)) words++;
    const long lost = (long)expect.n - (long)tap.n, wrong = tap.crc != expect.crc ? 1 : 0, allocations = (long)gAudioAllocations.load();
    if (std::getenv("XT_FX_FIRST") && (lost || wrong || short_ || words || allocations)) std::printf("  threads: %ld lost, %ld out of order, %ld waits cut short, %ld words, %ld allocations\n", lost, wrong, short_, words, allocations);
    if (!c.ok()) std::printf("  threads: a fault: %s\n", c.error().c_str());
    std::printf("control under threads: %zu edits, %ld different\n", edits.size(), (lost != 0) + wrong + short_ + words + allocations + (tap.overflow ? 1 : 0) + (c.ok() ? 0 : 1));
    (void)samples;
}

// What the waits and the mutes are for: a type change and a patch under a tone, the largest step between two samples against the tone's own, with the driver's waits and with the words all at once.
struct Immediate : XtFxControl::Bus {
    XtFxChip& chip; explicit Immediate(XtFxChip& x) : chip(x) {}
    void write(uint8_t c, uint16_t i, uint32_t w) override { chip.hostWord(c, i, w); }
    uint32_t read(uint16_t i) override { write(0x37, i, 0); uint32_t v = 0; for (uint32_t k = 0; k < 4; k++) v |= (uint32_t)chip.hostRead(k) << (8 * k); return v & 0x0fffffff; }
    void wait(uint32_t) override {}
};
double stepUnderTone(const char* rom, bool honoured, bool patch) {
    auto chip = std::make_unique<XtFxChip>(); Immediate imm(*chip);
    XtFxControl c(rom, honoured ? nullptr : &imm);
    c.set(0x120, 1); c.set(0x177, 11); c.loadPatch();                                                     // from a Stereo Delay, full of the tone, to an overdrive
    double last = 0, most = 0, tone = 0;
    for (int i = 0; i < 60000; i++) {
        if (i == 40000) { c.set(0x177, 5); if (patch) c.loadPatch(); else c.update(); }
        const int32_t v = (int32_t)std::lrint(0.25 * std::sin(2 * M_PI * 100 * i / 44100.0) * 8388608.0);
        c.service(*chip);
        for (unsigned s = 0x20; s <= 0x2e; s += 2) chip->setGram(s, s == 0x28 || s == 0x2a ? v : 0);
        chip->runFrame();
        const double y = (double)chip->gram(0xa1), d = std::fabs(y - last); last = y;
        if (i > 20000 && i < 40000) tone = std::max(tone, d);
        if (i >= 40000) most = std::max(most, d);
    }
    return db(most / tone);
}
void changeUnderTone(const char* rom) {
    std::printf("under a tone, the largest step against the tone's own: an MFX type change %+.1f dB, a patch %+.1f dB; with the words all at once, %+.1f dB, %+.1f dB\n",
                stepUnderTone(rom, true, false), stepUnderTone(rom, true, true), stepUnderTone(rom, false, false), stepUnderTone(rom, false, true));
}

// What each kind of change costs on the bus: a live edit stays small and doesn't wait, a value that hasn't changed costs nothing, a type change mutes before it swaps, a patch reloads.
void behaviour(const char* rom) {
    int tried = 0, different = 0; const bool first = std::getenv("XT_FX_FIRST"), show = std::getenv("XT_FX_COUNTS");
    const auto check = [&](bool good, const char* what) { tried++; if (!good) { different++; if (first) std::printf("  behaviour: %s\n", what); } };
    Recorder rec; XtFxControl c(rom, &rec);
    c.set(0x120, 1); c.set(0x121, 1); c.set(0x122, 1); c.set(0x177, 1); c.set(0x1fc, 1); c.set(0x230, 1); c.loadPatch();
    // A live edit: this many words on the bus, no fewer and no more (a reload or a ramp would add to them), and no wait.
    const auto live = [&](int id, int v, uint64_t words, const char* what) {
        const uint64_t n = rec.n; const size_t w = rec.waits.size(); c.set(id, v); c.update();
        if (show) std::printf("  %-20s %3llu words, %zu waits\n", what, (unsigned long long)(rec.n - n), rec.waits.size() - w);
        check(rec.n - n == words && rec.waits.size() == w, what);
    };
    live(0x55e + 4, 20, 16, "an MFX parameter"); live(0x7c7, 40, 1, "a Chorus parameter"); live(0x80e, 20, 3, "a Reverb parameter");
    live(0x178, 90, 4, "MFX Master"); live(0x179, 90, 1, "MFX Chorus Send"); live(0x17a, 90, 1, "MFX Reverb Send"); live(0x1fd, 90, 2, "Chorus Master"); live(0x1fe, 90, 1, "Chorus to Reverb"); live(0x231, 90, 2, "Reverb Master");
    live(0x3e, 90, 2, "Master Level"); live(0x51e, 1, 4, "Attenuation"); live(0x70, 0, 47, "EQ Switch"); live(0x70, 1, 7, "EQ Switch back"); live(0x66, 20, 7, "an EQ gain");
    // The tempo's debounce: a new tempo reaches the chip after holding for two reads, 80 ms apart, and a switch's edit goes at once and then waits one.
    const auto waits = [&](int id, int v, size_t count, const char* what) {
        const uint64_t n = rec.n; const size_t w = rec.waits.size(); c.set(id, v); c.update();
        if (show) std::printf("  %-20s %3llu words, %zu waits\n", what, (unsigned long long)(rec.n - n), rec.waits.size() - w);
        bool good = rec.waits.size() - w == count && rec.n > n; for (size_t i = w; i < rec.waits.size(); i++) good = good && rec.waits[i].second == 3528; check(good, what);
    };
    c.set(0x1fc, 6); c.update(); waits(0x11d, 1400, 2, "a new tempo"); waits(0x120, 0, 1, "MFX Switch"); waits(0x120, 1, 1, "MFX Switch back"); waits(0x121, 0, 1, "Chorus Switch"); waits(0x121, 1, 1, "Chorus Switch back"); waits(0x122, 0, 1, "Reverb Switch"); waits(0x122, 1, 1, "Reverb Switch back");
    { const uint64_t n = rec.n; const size_t w = rec.waits.size(); c.set(0x76, 3); c.update(); check(rec.n > n && rec.waits.size() > w, "Output Gain"); }
    // A value that hasn't changed: nothing at all.
    const auto same = [&](int id, const char* what) { const uint64_t n = rec.n; const size_t w = rec.waits.size(); c.set(id, c.get(id)); c.update(); check(rec.n == n && rec.waits.size() == w && !c.pending(), what); };
    same(0x178, "a level unchanged"); same(0x120, "a switch unchanged"); same(0x177, "a type unchanged"); same(0x3e, "Master unchanged"); same(0x1fc, "the Chorus's type unchanged");
    // A type change ramps down and waits before it loads; a patch mutes, and loads the same type again.
    { const uint64_t n = rec.n; c.set(0x177, 2); const size_t w = rec.waits.size(); c.update(); check(rec.waits.size() > w && rec.waits[w].first - n < 64 && rec.waits[w].second > 128, "an MFX type change doesn't ramp down and wait first"); }
    { const uint64_t n = rec.n; const size_t w = rec.waits.size(); c.loadPatch(); check(rec.waits.size() > w && rec.waits[w].first - n < 64 && rec.waits[w].second > 128, "a patch doesn't mute first"); if (show) std::printf("  a patch: %llu words\n", (unsigned long long)(rec.n - n)); check(rec.n - n > 2000, "a patch doesn't reload"); }
    // set() keeps to the descriptor's range, and everything the readers read counts as a change.
    c.set(0x177, 1000); check(c.get(0x177) == 41, "a type above its range"); c.set(0x177, -5); check(c.get(0x177) == 0, "a type below its range"); c.set(0x76, 99); check(c.get(0x76) == 12, "Output Gain above its range"); c.set(0x76, -99); check(c.get(0x76) == -12, "Output Gain below its range");
    c.set(0x8000, 1); check(c.get(0x8000) == 0, "an ID that isn't there");
    c.update();
    static const int fixed[] = { 0x11d, 0x120, 0x121, 0x122, 0x177, 0x178, 0x179, 0x17a, 0x1fc, 0x1fd, 0x1fe, 0x230, 0x231, 0x3e, 0x76, 0x51e, 0x41, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70 };
    int unseen = 0; const auto seen = [&](int id) { if (c.highest(id) == c.lowest(id)) return; const int v = c.get(id); c.set(id, v == c.highest(id) ? c.lowest(id) : v + 1); if (!c.pending()) unseen++; c.set(id, v); c.update(); };
    for (const int id : fixed) seen(id);
    for (int id = 0x55d; id <= 0x8c6; id++) seen(id);
    if (first && unseen) std::printf("  behaviour: %d IDs set() doesn't count\n", unseen);
    check(unseen == 0, "an ID the readers read that set() doesn't count");
    // The Matrix: each of the three reaches the types that assign it, and moving a parameter puts words on the bus.  An unchanged value, or a type that doesn't assign it, costs nothing.
    {
        int responds[3] = {}, thru = 0;
        for (int t = 0; t < 42; t++) for (int k = 0; k < 3; k++) {
            Recorder m; XtFxControl a(rom, &m); a.set(0x177, t); a.loadPatch();
            const uint64_t n = m.n; int v[3] = {}; v[k] = 20000; a.setMatrix(v[0], v[1], v[2]);
            if (m.n != n) responds[k]++;
            if (t == 0 && m.n != n) thru++;
            const uint64_t after = m.n; a.setMatrix(v[0], v[1], v[2]); check(m.n == after, "the Matrix, unchanged");
            v[k] = 0; a.setMatrix(v[0], v[1], v[2]); if (m.n != after) check(a.ok(), "the Matrix back to 0");
        }
        if (show) std::printf("  the Matrix: types that respond to PRM1 %d, PRM2 %d, PRM3 %d\n", responds[0], responds[1], responds[2]);
        check(responds[0] == 32 && responds[1] == 22 && responds[2] == 6, "the Matrix doesn't reach the types it should"); check(thru == 0, "Thru answers the Matrix");
    }
    // The catalog: the types and what each lists.  A step of each parameter, with its type loaded, goes on the bus, but for the eight that HARDWARE_FIRMWARE.md names.
    {
        const XtFxCatalog cat = xtFxCatalog(rom);
        check(cat.ok && cat.mfx.size() == 42 && cat.chorus.size() == 9 && cat.reverb.size() == 11 && cat.mfxTypes.size() == 42 && cat.chorusTypes.size() == 9 && cat.reverbTypes.size() == 11 && cat.eq.size() == 12, "the catalog's size");
        check(cat.ok && cat.mfxTypes[0] == "Thru" && cat.mfxTypes[3] == "Resonant Filter" && cat.mfxTypes[41] == "Pseudo Stereo", "the MFX types' names");
        int listed = 0, silent = 0, badName = 0, badRange = 0;
        const auto step = [&](Recorder& m, XtFxControl& a, const XtFxParam& p) {
            listed++;
            if (p.name.empty() || p.name[0] == ' ' || p.name.back() == ' ' || p.name.find('*') != std::string::npos) badName++;
            if (p.low != a.lowest(p.id) || p.high != a.highest(p.id) || p.low >= p.high) badRange++;
            const uint64_t n = m.n; const int v = a.get(p.id); a.set(p.id, v == p.high ? v - 1 : v + 1); a.update();
            if (m.n == n) silent++;
        };
        const auto types = [&](int typeId, int switchId, const std::vector<std::vector<XtFxParam>>& list) {
            for (size_t t = 0; t < list.size(); t++) { Recorder m; XtFxControl a(rom, &m); a.set(switchId, 1); a.set(typeId, (int)t); a.loadPatch(); for (const XtFxParam& p : list[t]) step(m, a, p); }
        };
        types(0x177, 0x120, cat.mfx); types(0x1fc, 0x121, cat.chorus); types(0x230, 0x122, cat.reverb);
        { Recorder m; XtFxControl a(rom, &m); for (const XtFxParam& p : cat.eq) step(m, a, p); }
        if (show) std::printf("  the catalog: %d parameters, %d silent\n", listed, silent);
        check(listed == 872, "the catalog lists the wrong parameters"); check(silent == 8, "the catalog's parameters on the bus"); check(badName == 0, "a name in the catalog"); check(badRange == 0, "a range in the catalog");
    }
    // The outside clock: the firmware's poll goes by Clock Source, and moves only past its margin, here 2.5 BPM plus 1% of 120.
    {
        Recorder m; XtFxControl a(rom, &m); a.set(0x120, 1); a.set(0x177, 11); a.loadPatch();
        const auto poll = [&](int ms, bool moves, const char* what) { const uint64_t n = m.n; a.outsideClock(ms); check((m.n != n) == moves && a.ok(), what); };
        poll(100, false, "the outside clock with Clock Source at 0");
        a.set(0x41, 1); a.update();
        poll(125, true, "the outside clock's first"); poll(125, false, "the outside clock, the same");
        poll(0, false, "the outside clock at no time");
        poll(122, false, "the outside clock within its margin, up"); poll(128, false, "the outside clock within its margin, down");
        poll(129, true, "the outside clock past its margin, down");
        poll(125, true, "the outside clock past its margin, up");
        a.set(0x41, 0); a.update();
        poll(90, false, "the outside clock with Clock Source back at 0");
    }
    // tick(): a message at a time, so a stream of edits doesn't run ahead of the chip by what the driver's waits add up to, and the last still lands.  And what it sends is what update() or loadPatch() would.
    {
        auto chip = std::make_unique<XtFxChip>(); XtFxControl a(rom); a.set(0x120, 1); a.set(0x177, 11); a.loadPatch();
        while (!a.drained()) { a.pump(); a.service(*chip); chip->runFrame(); }
        int tempo = 0;
        for (int i = 0; i < 200; i++) { tempo = 600 + i * 7; a.set(0x11d, tempo); a.tick(); a.pump(); for (int s = 0; s < 441; s++) { a.service(*chip); chip->runFrame(); } }
        long behind = 0;
        for (; !a.drained() && behind < 88200; behind++) { a.pump(); a.service(*chip); chip->runFrame(); }
        a.tick();
        for (; !a.drained() && behind < 88200; behind++) { a.pump(); a.service(*chip); chip->runFrame(); }
        if (show) std::printf("  a stream of edits: %.2f s behind at its end\n", (double)behind / 44100.0);
        check(behind < 17640, "a stream of edits runs ahead of the chip");
        XtFxControl b(rom); b.set(0x120, 1); b.set(0x177, 11); b.set(0x11d, tempo); b.loadPatch();
        int d = 0; for (unsigned i = 0; i < 2048; i++) if (a.word(i) != b.word(i)) d++;
        check(d == 0, "the last of a stream of edits");
        Recorder x, y; XtFxControl e(rom, &x), f(rom, &y);
        e.set(0x177, 11); f.set(0x177, 11); e.wantPatch(); e.tick(); f.loadPatch(); check(x.crc == y.crc, "a patch by tick()");
        const uint64_t n = x.n; e.tick(); check(x.n == n, "tick() sends a patch twice");
        e.set(0x178, 90); f.set(0x178, 90); e.tick(); f.update(); check(x.crc == y.crc && x.n != n, "an edit by tick()");
    }
    // What's the system's survives a patch, and the start reads it.
    { XtFxControl a(rom), b(rom); a.set(0x3e, 64); a.update(); a.loadPatch(); b.set(0x3e, 64); b.loadPatch(); int d = 0; for (unsigned i = 0; i < 2048; i++) if (a.word(i) != b.word(i)) d++; check(d == 0, "a system edit lost to a patch"); }
    { XtFxControl a(rom), b(rom); b.set(0x70, 0); b.update(); b.set(0x70, 1); b.update(); int d = 0; for (unsigned i = 0; i < 2048; i++) if (a.word(i) != b.word(i)) d++; check(d == 0, "the start doesn't read the system"); }
    // Each thing the check counts on, a type at the end of its range: types 41, 8 and 10 load.
    { XtFxControl a(rom); a.set(0x177, 41); a.set(0x1fc, 8); a.set(0x230, 10); a.update(); check(a.ok(), "the last types fault"); }
    // The waits and the mutes do what they're for.  (0 dB would be no bigger than the tone's own step; without them it's over 30.)
    check(stepUnderTone(rom, true, false) < 6.0, "a type change under a tone clicks"); check(stepUnderTone(rom, true, true) < 6.0, "a patch under a tone clicks");
    if (!c.ok() && first) std::printf("  behaviour: a fault: %s\n", c.error().c_str());
    std::printf("control's behaviour on the bus: %d samples, %d different\n", tried, different + (c.ok() ? 0 : 1));
}

// The panel's text.  Every value of every parameter against the firmware's routine itself, without the unit and with it.  What the plugin does to the text is small: the note values' names, the spaces, and a
// space and no brackets for the unit, which goes only on a number.  What's left over is the three of Non-Linear whose strings are a row out.  Then the note names against what the driver does at a tempo,
// and each value's text typed back in.
void valueText(const char* rom) {
    const XtFxCatalog cat = xtFxCatalog(rom);
    Sh4 fw; fw.loadRom(rom); fw.mapRam(0x8C000000, 0x00100000);
    int tried = 0, different = 0; const bool first = std::getenv("XT_FX_FIRST") != nullptr;
    const auto check = [&](bool good, const char* what, int id = 0, int v = 0, const std::string& got = {}) { tried++; if (!good) { different++; if (first) std::printf("  value text: %s, ID %x value %d: \"%s\"\n", what, id, v, got.c_str()); } };
    const auto routine = [&](uint32_t fn, int id, int v) {
        uint32_t at = 0; std::string t;
        if (!fw.call(fn, { (uint32_t)id, (uint32_t)v }, 0x8C0E0000, &at, 2000000)) return std::string("a fault");
        for (uint8_t ch = fw.rd8(at); ch; ch = fw.rd8(++at)) t += (char)ch;
        return t;
    };
    const auto isNumber = [](const std::string& t) {
        size_t i = t[0] == '-' || t[0] == '+' ? 1 : 0, digits = 0, dots = 0;
        for (; i < t.size(); i++) { if (t[i] == '.') dots++; else if (t[i] >= '0' && t[i] <= '9') digits++; else return false; }
        return digits > 0 && dots <= 1;
    };
    static const std::map<std::string, std::string> notes = { { "\x11", "1/16" }, { "\x12", "1/8" }, { "\x18", "1/4" }, { "\x13", "1/2" }, { "\x14", "1/1" }, { "\x15", "2/1" } };
    int kinds[4] = {}, values = 0, outOfRow = 0;
    const auto param = [&](const XtFxParam& p) {
        for (int v = p.low; v <= p.high; v++) {
            values++;
            const std::string bare = routine(0x89462120, p.id, v), with = routine(0x89462130, p.id, v), shown = cat.values->text(p.id, v);
            check(with.compare(0, bare.size(), bare) == 0 && with.size() >= bare.size(), "the routine with a unit doesn't start as it does without", p.id, v, with);
            std::string unit = with.substr(std::min(bare.size(), with.size()));
            if (p.id >= 0x8b6 && p.id <= 0x8b8) { unit = p.id == 0x8b6 ? "[ms]" : p.id == 0x8b7 ? "[%]" : "[Hz]"; outOfRow++; }
            const auto note = notes.find(bare.substr(0, 1));
            std::string want;
            if (note != notes.end()) { want = note->second + (bare.substr(1) == "\x17" ? "T" : bare.substr(1) == "." ? "." : ""); kinds[0]++; }
            else {
                const size_t a = bare.find_first_not_of(' '), b = bare.find_last_not_of(' ');
                want = a == std::string::npos ? "" : bare.substr(a, b - a + 1);
                if (isNumber(want) && unit.size() > 2) { want += " " + unit.substr(1, unit.size() - 2); kinds[1]++; } else kinds[isNumber(want) ? 2 : 3]++;
            }
            check(!want.empty() && shown == want, "the text", p.id, v, shown);
            check(cat.values->text(p.id, cat.values->value(p.id, p.low, p.high, shown, p.low == v ? p.high : p.low)) == shown, "the text typed back", p.id, v, shown);
        }
    };
    for (const auto* list : { &cat.mfx, &cat.chorus, &cat.reverb }) for (const auto& type : *list) for (const XtFxParam& q : type) param(q);
    for (const XtFxParam& q : cat.eq) param(q);
    if (first) std::printf("  value text: %d values: %d notes, %d numbers with units, %d without, %d names; %d out of row\n", values, kinds[0], kinds[1], kinds[2], kinds[3], outOfRow);
    check(values == 53128, "the values");

    // Typing: what the box takes besides a value's own text.
    const auto typed = [&](int id, int low, int high, const char* text, const char* want) { const std::string got = cat.values->text(id, cat.values->value(id, low, high, text, low)); check(got == want, text, id, 0, got); };
    typed(0x55e, 0, 19, "200", "200 Hz"); typed(0x55e, 0, 19, "200hz", "200 Hz"); typed(0x55e, 0, 19, "210", "200 Hz");         // Parametric EQ's Low Freq: a number, the nearest, in any case
    typed(0x55f, 0, 30, "+6", "+6 dB"); typed(0x55f, 0, 30, "-6 dB", "-6 dB"); typed(0x55f, 0, 30, "6", "+6 dB"); typed(0x55f, 0, 30, "-6", "-6 dB");    // its Low Gain
    typed(0x57a, 0, 127, "1/16.", "1/16."); typed(0x57a, 0, 127, "1/8t", "1/8T"); typed(0x57a, 0, 127, "2.03", "2.05 Hz");       // Sweep Rate
    typed(0x5de, 0, 121, "2", "0 ms");                                                                                           // Delay Time: 2/1 isn't the number 2
    typed(0x578, 0, 5, "mid+h", "MID+HIGH");                                                                                     // Band Mode: the start of a name
    check(cat.values->value(0x578, 0, 5, "nothing", 3) == 3 && cat.values->value(0x578, 0, 5, " ", 3) == 3, "what it doesn't know stays where it was");
    typed(0x5f7, 0, 126, "l20", "L20"); typed(0x5f7, 0, 126, " L 20 ", "L20"); typed(0x5f7, 0, 126, "20r", "20R");               // a Pan
    typed(0x7c2, 0, 16, "THRU", "THRU"); typed(0x7c2, 0, 16, "2100", "2000Hz");                                                  // Chorus 1's Pre Low Freq, with the firmware's own Hz
    typed(0x8ae, 0, 110, "95.4%", "95 %");                                                                                      // Env Time Ratio

    // The note names, against the driver: at a tempo, a Rate's coefficient goes as its Hz, and a note's Hz is the beats per second over the note's beats.  Sweep Rate has all sixteen.
    struct Coef : XtFxControl::Bus {
        std::map<int, uint32_t> last;
        void write(uint8_t command, uint16_t index, uint32_t word) override { if (command == 0x35) last[index] = word; }
        uint32_t read(uint16_t) override { return 0; }
        void wait(uint32_t) override {}
    };
    const auto number = [](uint32_t word) { return (double)((word & 0xff) << 8 | (word >> 16 & 1) << 7 | (word >> 9 & 0x7e)); };   // the bus's encoding of a coefficient (XtFxControl::hooks)
    const auto beatsOf = [](const std::string& n) { const size_t s = n.find('/'); double b = 4.0 * std::atof(n.c_str()) / std::atof(n.c_str() + s + 1); if (n.back() == 'T') b *= 2.0 / 3; if (n.back() == '.') b *= 1.5; return b; };
    int measured = 0;
    for (const int tempo : { 1200, 900 }) {
        Coef bus; XtFxControl a(rom, &bus); a.set(0x11d, tempo); a.set(0x120, 1); a.set(0x177, 3); a.loadPatch();
        const auto rate = [&](int v) { a.set(0x57a, v); a.update(); return number(bus.last[0x5ca]); };
        const double perHz = rate(19) / 1.0;                                                                                      // value 19 is 1.00 Hz
        check(cat.values->text(0x57a, 19) == "1.00 Hz", "Sweep Rate's 1.00 Hz", 0x57a, 19, cat.values->text(0x57a, 19));
        for (int v = 112; v <= 127; v++) {
            const std::string n = cat.values->text(0x57a, v);
            const double want = tempo / 600.0 / beatsOf(n), got = rate(v) / perHz;
            if (first) std::printf("  note %3d %-6s at %4.1f BPM: %6.3f Hz, %6.3f by its name\n", v, n.c_str(), tempo / 10.0, got, want);
            check(n.find('/') != std::string::npos && std::abs(got / want - 1) < 0.01, "a note's name against the driver", 0x57a, v, n);
            measured++;
        }
    }
    // And a delay's coefficient goes as its time: Stereo Delay's Delay Time, those that fit under its 650 ms at 120 BPM.
    {
        Coef bus; XtFxControl a(rom, &bus); a.set(0x11d, 1200); a.set(0x120, 1); a.set(0x177, 11); a.loadPatch();
        const auto time = [&](int v) { a.set(0x5de, v); a.update(); return number(bus.last[0x5c8]); };
        const double perMs = time(20) / 100.0;                                                                                    // value 20 is 100 ms
        check(cat.values->text(0x5de, 20) == "100 ms", "Delay Time's 100 ms", 0x5de, 20, cat.values->text(0x5de, 20));
        for (int v = 106; v <= 112; v++) {
            const std::string n = cat.values->text(0x5de, v);
            const double want = 500.0 * beatsOf(n), got = time(v) / perMs;
            if (first) std::printf("  note %3d %-6s at 120 BPM: %6.1f ms, %6.1f by its name\n", v, n.c_str(), got, want);
            check(n.find('/') != std::string::npos && std::abs(got / want - 1) < 0.01, "a note's name against the delay", 0x5de, v, n);
            measured++;
        }
    }
    check(measured == 39, "the notes measured");
    std::printf("control's value text against the firmware's routine and driver: %d samples, %d different\n", tried, different);
}

// ------------------------------------------------------------------ the unit
// The frame's way in, against the firmware's own: 0x894db2b0 truncates the bus to int32, and the chip has the top 24 bits.
void unitConversion(const char* rom, uint32_t seed) {
    Sh4 c; if (!c.loadRom(rom)) return; c.mapRam(0x8C000000, 0x100000);
    Rng r(seed); long tried = 0, different = 0; float firstX = 0; int32_t firstFw = 0, firstPort = 0;
    for (int round = 0; round < 200; round++) {
        float in[256];
        for (float& x : in) {
            const int k = r.below(6);                                            // everything a sum of tones can be: tiny, around a word's step, ordinary, around full scale and past it
            const float u = (float)(int32_t)r.next() / 2147483648.0f;
            x = k == 0 ? u * 1e-6f : k == 1 ? u * 1024.0f : k == 2 ? u * 1e6f : k == 3 ? u * 2147483648.0f : k == 4 ? u * 1e12f : (u < 0 ? -2147483648.0f : 2147483648.0f) * (1.0f + (float)r.below(3) * 1e-6f - 1e-6f);
        }
        for (uint32_t i = 0; i < 256; i++) { uint32_t b; std::memcpy(&b, &in[i], 4); c.wr32(0x89d2cb10 + 4 * i, b); }
        if (!c.call(0x894db2b0, {}, kSp)) { std::printf("fault in the truncation: %s\n", c.error().c_str()); different++; break; }
        for (uint32_t i = 0; i < 256; i++) {
            const int32_t fw = (int32_t)c.rd32(0x89cf7a78 + 4 * i) >> 8, port = XtFxUnit::toChip(in[i] * 16.0f);      // toChip has the tone's Out Gain in it too, a sixteenth
            tried++;
            if (fw != port) { if (!different) { firstX = in[i]; firstFw = fw; firstPort = port; } different++; }
        }
    }
    if (different && std::getenv("XT_FX_FIRST")) std::printf("  unit: %.9g to the chip: firmware %d, port %d\n", firstX, firstFw, firstPort);
    std::printf("the frame into the chip against the firmware's truncation: %ld samples, %ld different\n", tried, different);
}

// Through the unit: each word of the frame to its place, at the level the plugin has without the chip, and how long it takes.
struct UnitLine { double left = 0, right = 0; int delay = -1; };
UnitLine unitLine(const char* rom, int word, bool eq) {
    auto u = std::make_unique<XtFxUnit>(rom);
    u->control().set(0x70, eq ? 1 : 0); u->control().update(); u->settle();
    float frame[8] = {}, lr[2] = {};
    UnitLine out; double in = 0;
    for (int i = 0; i < 20000; i++) {
        const float v = (float)(0.25 * std::sin(2 * M_PI * 1000 * i / 44100.0)) * 0x1p31f;
        frame[word] = v; u->process(frame, lr, 1);
        if (i > 10000) { in = std::max(in, std::fabs((double)v) * 0x1p-31); out.left = std::max(out.left, std::fabs((double)lr[0])); out.right = std::max(out.right, std::fabs((double)lr[1])); }
    }
    // the delay: a click, in a chip that's settled.  The DC cut's tail is much smaller.
    frame[word] = 0; for (int i = 0; i < 2000; i++) u->process(frame, lr, 1);
    for (int i = 0; i < 64; i++) { frame[word] = i == 0 ? 0x1p30f : 0.0f; u->process(frame, lr, 1); if (out.delay < 0 && (std::fabs(lr[0]) > 0.1f || std::fabs(lr[1]) > 0.1f)) out.delay = i; }
    out.left /= in; out.right /= in;
    return out;
}
void unitLevels(const char* rom, bool print) {
    int tried = 0, different = 0;
    static const char* const kName[8] = { "MAIN L", "MAIN R", "Reverb send", "Chorus send", "MULTI L", "MULTI R", "DIR L", "DIR R" };
    const auto near = [](double x, double to, double tol) { return std::fabs(x - to) <= tol; };
    for (int w = 0; w < 8; w++) {
        const UnitLine a = unitLine(rom, w, false);
        const bool isLeft = w == 0 || w == 4 || w == 6, isSend = w == 2 || w == 3;
        tried++;
        const bool good = isSend ? a.left < 1e-4 && a.right < 1e-4 : near(db(isLeft ? a.left : a.right), 0.0, 0.05) && (isLeft ? a.right : a.left) < 1e-4 && a.delay == 1;
        if (!good) { different++; if (std::getenv("XT_FX_FIRST")) std::printf("  unit: %s: left %.3f dB, right %.3f dB\n", kName[w], db(a.left), db(a.right)); }
        if (print) std::printf("through the unit, %s: left %+.2f dB, right %+.2f dB, %d samples late\n", kName[w], db(a.left), db(a.right), a.delay);
    }
    const UnitLine e = unitLine(rom, 4, true);
    if (print) std::printf("through the unit, MULTI L with the system's EQ on at its defaults: %+.2f dB\n", db(e.left));
    {   // With it running, an edit reaches the chip through the queue: Master Level down, and the level follows.
        auto u = std::make_unique<XtFxUnit>(rom);
        u->settle();
        float frame[8] = {}, lr[2] = {};
        const auto level = [&](int n) { double top = 0; for (int i = 0; i < n; i++) { frame[4] = (float)(0.25 * std::sin(2 * M_PI * 1000 * i / 44100.0)) * 0x1p31f; u->process(frame, lr, 1); if (i > n / 2) top = std::max(top, std::fabs((double)lr[0])); } return top; };
        const double before = level(4000);
        u->control().set(0x3e, 64); u->control().update();
        const double after = level(4000);
        tried++;
        if (!(after < before * 0.5)) { different++; if (std::getenv("XT_FX_FIRST")) std::printf("  unit: Master Level moved, from %.3f to %.3f\n", before, after); }
    }
    if (!print) std::printf("the frame through the unit: %d samples, %d different\n", tried, different);
}
} // namespace

// The audio thread must not allocate.  Only this harness replaces these.
void* operator new(std::size_t n) { if (tAudioThread) gAudioAllocations++; void* p = std::malloc(n ? n : 1); if (!p) std::abort(); return p; }
void* operator new[](std::size_t n) { if (tAudioThread) gAudioAllocations++; void* p = std::malloc(n ? n : 1); if (!p) std::abort(); return p; }
void operator delete(void* p) noexcept { if (tAudioThread && p) gAudioAllocations++; std::free(p); }
void operator delete[](void* p) noexcept { if (tAudioThread && p) gAudioAllocations++; std::free(p); }
void operator delete(void* p, std::size_t) noexcept { if (tAudioThread && p) gAudioAllocations++; std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { if (tAudioThread && p) gAudioAllocations++; std::free(p); }

int main(int argc, char** argv) {
    if (argc < 2) { std::fprintf(stderr, "xt_fx <rom> [frames] [seed] | numbers | speed | control [events] [seed]\n"); return 2; }
    const char* rom = argv[1];
    if (argc > 2 && !std::strcmp(argv[2], "numbers")) { numbers(rom); changeUnderTone(rom); unitLevels(rom, true); return 0; }
    if (argc > 2 && !std::strcmp(argv[2], "threads")) { threads(rom, argc > 3 ? std::atoi(argv[3]) : 200, argc > 4 ? (uint32_t)std::atoi(argv[4]) : 2024); return 0; }
    if (argc > 2 && !std::strcmp(argv[2], "control")) {
        const int events = argc > 3 ? std::atoi(argv[3]) : 200; const uint32_t seed = argc > 4 ? (uint32_t)std::atoi(argv[4]) : 2024;
        readers(rom, seed); behaviour(rom); valueText(rom); liveEdits(rom, events, seed); threads(rom, events, seed); againstReference(rom, events, seed); unitConversion(rom, seed); unitLevels(rom, false);
        return 0;
    }
    if (argc > 2 && !std::strcmp(argv[2], "speed")) { speed(rom); return 0; }
    const int frames = argc > 2 ? std::atoi(argv[2]) : 8192; const uint32_t seed = argc > 3 ? (uint32_t)std::atoi(argv[3]) : 2024;
    { Sh4 c; c.loadRom(rom); for (int t = 0; t < 42; t++) { char nm[24]; c.read(0x89788c98 + 24 * (uint32_t)t, nm, 20); nm[20] = 0; for (int e = 19; e > 0 && nm[e] == ' '; e--) nm[e] = 0; compare(rom, ("MFX " + std::to_string(t) + " " + nm).c_str(), t, t % 11, t % 9, frames, seed + (uint32_t)t, false); } }
    for (int t = 0; t < 11; t++) compare(rom, (std::string("Reverb ") + kRevName[t]).c_str(), 0, t, 0, frames, seed + 100 + (uint32_t)t, false);
    for (int t = 0; t < 9; t++) compare(rom, (std::string("Chorus ") + kChoName[t]).c_str(), 1, 0, t, frames, seed + 200 + (uint32_t)t, false);
    compare(rom, "every type, changing under load", 0, 0, 0, frames * 8, seed + 300, true);
    return 0;
}
#endif
