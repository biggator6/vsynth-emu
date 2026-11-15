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
// one-off: a core's 768 clocks, the plugin's hottest loop (every sample, both cores), its state kept in registers whole
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
    // The last twelve clocks: words multiply into A undecoded, by what the raw registers hold and then by the words the host
    // last touched.  What it leaves in the accumulators' history, the next sample's first stores read.  Both cores (v204):
    // core 0 runs its words in order, so its tail starts at 756.  Until v204 core 0's wasn't run, as nothing had reached it
    // (r0 and r255 stayed 0, and the words before it were empty); the outside input's routing does, in program A.
    if constexpr (K == 0) pc = kDecoded;
    for (unsigned t = 0; t < kRun - kDecoded; t++) {
        if (pc < kRun) {
            pc++;
            if (t < 2) lastIn = sext(iram[(pos + (t ? 0xffu : 0u)) & 255], 24);
            else if (hostTail_[K] >= 0) {
                const uint32_t a = (uint32_t)hostTail_[K] + (t - 2) / 2;
                const uint32_t w = words_[K][hostTail_[K] < (int32_t)kRun ? std::min(a, kRun - 1) : a & 0x3ff];
                const int64_t product = (int64_t)lastIn * sext((int32_t)(w & 255), 8);
                acc[0] = sext(acc[0] + (int32_t)(product >> shiftOf(w >> 8 & 3)), 30);
            }
        }
        hist[0][h & 3] = acc[0];
        hist[1][h & 3] = acc[1];
        h = (h + 1) & 3;
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
