// SH-4 interpreter — see Sh4Interp.h.  Instruction semantics follow the
// SH-4 software manual.  Float arithmetic runs on the host in single or double
// precision with no contraction (FMAC = product, then sum), in the rounding
// mode FPSCR.RM selects; FIPR / FTRV form their sums in double and round once.
// SZ = 1 pair moves keep FR(n) (the high word of DR(n)) at the higher address,
// as Ghidra's little-endian SH-4 model does for DR registers.
#include "Sh4Interp.h"

#include <cfenv>
#include <cmath>
#include <cstdio>
#include <cstring>

#pragma clang fp contract(off)
#pragma STDC FENV_ACCESS ON

namespace {
constexpr uint32_t kSentinel = 0x8EFFFFF0u;     // return address of call(); never mapped as code
constexpr uint32_t kPageBits = 12;
inline uint32_t phys(uint32_t a) { return a & 0x1fffffffu; }
inline float asF(uint32_t u) { float f; std::memcpy(&f, &u, 4); return f; }
inline uint32_t asU(float f) { uint32_t u; std::memcpy(&u, &f, 4); return u; }
inline int32_t s8(uint32_t v) { return (int8_t)(uint8_t)v; }
inline int32_t s16(uint32_t v) { return (int16_t)(uint16_t)v; }
inline int32_t ftrcF(double x) {                  // FTRC: truncate, saturate
    if (x != x) return (int32_t)0x80000000;
    if (x >= 2147483648.0) return 0x7fffffff;
    if (x <= -2147483649.0) return (int32_t)0x80000000;
    return (int32_t)x;
}
} // namespace

Sh4::Sh4() : page(1u << (29 - kPageBits), nullptr), hookPage(1u << (29 - kPageBits), 0) {}
Sh4::~Sh4() { for (Region* g : regions) delete g; }

void Sh4::addRegion(uint32_t base, uint32_t size) {
    Region* g = new Region{ phys(base), size, std::vector<uint8_t>(size, 0) };
    regions.push_back(g);
    for (uint32_t o = 0; o < size; o += 1u << kPageBits) page[(g->base + o) >> kPageBits] = g->mem.data() + o;
}

void Sh4::mapRam(uint32_t addr, uint32_t size) {
    addr &= ~((1u << kPageBits) - 1);
    size = (size + (1u << kPageBits) - 1) & ~((1u << kPageBits) - 1);
    addRegion(addr, size);
}

bool Sh4::loadRom(const char* path) {
    FILE* f = path ? std::fopen(path, "rb") : nullptr;
    if (!f) { err = "cannot open rom"; return false; }
    std::fseek(f, 0, SEEK_END);
    const long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    const uint32_t base = 0x89300000u, end = 0x8a020000u;   // image, .data and .bss
    if (sz <= 0 || sz > (long)(end - base)) { std::fclose(f); err = "unexpected rom size"; return false; }
    addRegion(base, end - base);
    const bool ok = std::fread(at(base, (uint32_t)sz), 1, (size_t)sz, f) == (size_t)sz;
    std::fclose(f);
    if (!ok) { err = "cannot read rom"; return false; }
    // C startup (entry 0x89300040): .data image ROM [0x89834018, 0x8985858c)
    // → RAM [0x8985858c, 0x8987cb00); zero [0x8987cb00, 0x8a01ceb4).
    std::memmove(at(0x8985858cu, 0x8987cb00u - 0x8985858cu), at(0x89834018u, 0x8985858cu - 0x89834018u), 0x8985858cu - 0x89834018u);
    std::memset(at(0x8987cb00u, 0x8a01ceb4u - 0x8987cb00u), 0, 0x8a01ceb4u - 0x8987cb00u);
    return true;
}

uint8_t* Sh4::at(uint32_t a, uint32_t n) {
    const uint32_t p = phys(a);
    uint8_t* b = page[p >> kPageBits];
    if (!b) return nullptr;
    const uint32_t off = p & ((1u << kPageBits) - 1);
    if (off + n > (1u << kPageBits)) {   // spans pages: must be contiguous in one region
        for (Region* g : regions)
            if (p >= g->base && p + n <= g->base + g->size) return g->mem.data() + (p - g->base);
        return nullptr;
    }
    return b + off;
}

uint8_t* Sh4::ptr(uint32_t a, uint32_t n) { return at(a, n); }
uint32_t Sh4::rd32(uint32_t a) { uint8_t* p = at(a, 4); uint32_t v = 0; if (p) std::memcpy(&v, p, 4); else fault("read32", a); return v; }
uint16_t Sh4::rd16(uint32_t a) { uint8_t* p = at(a, 2); uint16_t v = 0; if (p) std::memcpy(&v, p, 2); else fault("read16", a); return v; }
uint8_t Sh4::rd8(uint32_t a) { uint8_t* p = at(a, 1); if (!p) { fault("read8", a); return 0; } return *p; }
void Sh4::wr32(uint32_t a, uint32_t v) { uint8_t* p = at(a, 4); if (p) std::memcpy(p, &v, 4); else fault("write32", a); }
void Sh4::wr16(uint32_t a, uint16_t v) { uint8_t* p = at(a, 2); if (p) std::memcpy(p, &v, 2); else fault("write16", a); }
void Sh4::wr8(uint32_t a, uint8_t v) { uint8_t* p = at(a, 1); if (p) *p = v; else fault("write8", a); }
void Sh4::write(uint32_t a, const void* src, size_t n) {
    const uint8_t* s = (const uint8_t*)src;
    for (size_t i = 0; i < n; ) { uint32_t chunk = (uint32_t)std::min<size_t>(n - i, 4096 - ((a + i) & 4095)); uint8_t* p = at(a + (uint32_t)i, chunk); if (!p) { fault("write", a + (uint32_t)i); return; } std::memcpy(p, s + i, chunk); i += chunk; }
}
void Sh4::read(uint32_t a, void* dst, size_t n) {
    uint8_t* d = (uint8_t*)dst;
    for (size_t i = 0; i < n; ) { uint32_t chunk = (uint32_t)std::min<size_t>(n - i, 4096 - ((a + i) & 4095)); uint8_t* p = at(a + (uint32_t)i, chunk); if (!p) { fault("read", a + (uint32_t)i); std::memset(d + i, 0, n - i); return; } std::memcpy(d + i, p, chunk); i += chunk; }
}

bool Sh4::fault(const char* what, uint32_t addr) {
    if (err.empty()) { char b[96]; std::snprintf(b, sizeof b, "%s at %08x (pc %08x)", what, addr, pc); err = b; }
    return false;
}

void Sh4::hook(uint32_t addr, std::function<void(Sh4&)> fn) {
    hooks[addr] = std::move(fn);
    hookPage[phys(addr) >> kPageBits] = 1;
}

void Sh4::syncRound() {
    const int want = (!forceNearest && (fpscr & 3) == 1) ? FE_TOWARDZERO : FE_TONEAREST;
    if (want != hostRound) { std::fesetround(want); hostRound = want; }
}
void Sh4::setFpscr(uint32_t v) { fpscr = v & 0x003fffffu; syncRound(); }

float Sh4::F(int n) { return asF(FR(n)); }
void Sh4::setF(int n, float v) { FR(n) = asU(v); }
double Sh4::D(int n) { uint64_t u = ((uint64_t)FR(n & ~1) << 32) | FR((n & ~1) + 1); double d; std::memcpy(&d, &u, 8); return d; }
void Sh4::setD(int n, double v) { uint64_t u; std::memcpy(&u, &v, 8); FR(n & ~1) = (uint32_t)(u >> 32); FR((n & ~1) + 1) = (uint32_t)u; }

bool Sh4::call(uint32_t addr, std::initializer_list<uint32_t> args, uint32_t sp, uint32_t* r0out, uint64_t maxSteps) {
    int i = 4;
    for (uint32_t a : args) if (i <= 7) r[i++] = a;
    r[15] = sp;
    r[0] = 0x00040001u;                 // some players take FPSCR from r0
    pr = kSentinel;
    pc = addr;
    const int savedRound = std::fegetround();
    hostRound = -1;
    setFpscr(0x00040001u);
    err.clear();
    const uint64_t limit = nsteps + maxSteps;
    bool ok = true;
    while (pc != kSentinel) {
        if (hookPage[phys(pc) >> kPageBits]) {
            auto h = hooks.find(pc);
            if (h != hooks.end()) { h->second(*this); continue; }
        }
        if (!step() || !err.empty()) { ok = false; break; }
        if (nsteps > limit) { err = "step limit"; ok = false; break; }
    }
    std::fesetround(savedRound);
    hostRound = -1;
    if (r0out) *r0out = r[0];
    return ok && err.empty();
}

bool Sh4::step() {
    const uint16_t op = rd16(pc);
    if (!err.empty()) return false;
    uint32_t next = pc + 2;
    ++nsteps;
    if (!exec(op, false, next)) return false;
    pc = next;
    return true;
}

// Executes one instruction at pc.  Delayed branches run their slot (at pc+2)
// before setting next.  For a slot instruction, `slot` is true and pc is the
// slot's address.
bool Sh4::exec(uint16_t op, bool slot, uint32_t& next) {
    const int n = (op >> 8) & 15, m = (op >> 4) & 15;
    uint32_t& Rn = r[n];
    const uint32_t Rm = r[m];
    auto T = [&]() -> uint32_t { return sr & 1; };
    auto setT = [&](bool b) { sr = (sr & ~1u) | (b ? 1u : 0u); };
    auto delayed = [&](uint32_t target) -> bool {    // run the slot, then branch
        if (slot) return fault("branch in delay slot", pc);
        const uint32_t save = pc;
        pc = save + 2;
        const uint16_t s = rd16(pc);
        uint32_t dummy = pc + 2;
        ++nsteps;
        if (!exec(s, true, dummy)) return false;
        pc = save;
        next = target;
        return true;
    };
    switch (op >> 12) {
    case 0x0:
        switch (op & 0xf) {
        case 0x2:
            switch (m) {
            case 0: Rn = sr; return true;
            case 1: Rn = gbr; return true;
            case 2: Rn = vbr; return true;
            case 3: Rn = ssr; return true;
            case 4: Rn = spc; return true;
            default: if (m >= 8) { Rn = rbank[m & 7]; return true; } break;
            }
            break;
        case 0x3:
            if (m == 0) { const uint32_t t = pc + 4 + Rn; pr = pc + 4; return delayed(t); }   // bsrf
            if (m == 2) { const uint32_t t = pc + 4 + Rn; return delayed(t); }                 // braf
            if (m == 8 || m == 9 || m == 0xa || m == 0xb) return true;                          // pref, ocbi, ocbp, ocbwb
            if (m == 0xc) { wr32(Rn, r[0]); return true; }                                      // movca.l
            break;
        case 0x4: wr8(Rn + r[0], (uint8_t)Rm); return true;
        case 0x5: wr16(Rn + r[0], (uint16_t)Rm); return true;
        case 0x6: wr32(Rn + r[0], Rm); return true;
        case 0x7: macl = Rn * Rm; return true;
        case 0x8:
            switch (op) {
            case 0x0008: setT(false); return true;
            case 0x0018: setT(true); return true;
            case 0x0028: mach = macl = 0; return true;
            case 0x0048: sr &= ~2u; return true;
            case 0x0058: sr |= 2u; return true;
            }
            break;
        case 0x9:
            if (op == 0x0009) return true;
            if (op == 0x0019) { sr &= ~(0x300u | 1u); return true; }                          // div0u
            if (m == 2) { Rn = T(); return true; }                                              // movt
            break;
        case 0xa:
            switch (m) {
            case 0: Rn = mach; return true;
            case 1: Rn = macl; return true;
            case 2: Rn = pr; return true;
            case 5: Rn = fpul; return true;
            case 6: Rn = fpscr; return true;
            case 0xf: Rn = dbr; return true;
            }
            break;
        case 0xb:
            if (op == 0x000b) return delayed(pr);                                               // rts
            break;
        case 0xc: Rn = (uint32_t)s8(rd8(Rm + r[0])); return true;
        case 0xd: Rn = (uint32_t)s16(rd16(Rm + r[0])); return true;
        case 0xe: Rn = rd32(Rm + r[0]); return true;
        case 0xf: {                                                                             // mac.l @Rm+,@Rn+
            const int64_t a = (int32_t)rd32(Rn), b = (int32_t)rd32(r[m]);
            Rn += 4; r[m] += 4;
            int64_t acc = (int64_t)(((uint64_t)mach << 32) | macl) + a * b;
            if (sr & 2) { const int64_t lim = (int64_t)1 << 47; if (acc >= lim) acc = lim - 1; if (acc < -lim) acc = -lim; }
            mach = (uint32_t)((uint64_t)acc >> 32); macl = (uint32_t)acc;
            return true;
        }
        }
        break;
    case 0x1: wr32(Rn + (op & 0xf) * 4, Rm); return true;
    case 0x2:
        switch (op & 0xf) {
        case 0x0: wr8(Rn, (uint8_t)Rm); return true;
        case 0x1: wr16(Rn, (uint16_t)Rm); return true;
        case 0x2: wr32(Rn, Rm); return true;
        case 0x4: wr8(Rn - 1, (uint8_t)Rm); Rn -= 1; return true;
        case 0x5: wr16(Rn - 2, (uint16_t)Rm); Rn -= 2; return true;
        case 0x6: wr32(Rn - 4, Rm); Rn -= 4; return true;
        case 0x7: {                                                                             // div0s
            const uint32_t q = Rn >> 31, mm = Rm >> 31;
            sr = (sr & ~0x300u) | (q << 8) | (mm << 9);
            setT(q != mm);
            return true;
        }
        case 0x8: setT((Rn & Rm) == 0); return true;
        case 0x9: Rn &= Rm; return true;
        case 0xa: Rn ^= Rm; return true;
        case 0xb: Rn |= Rm; return true;
        case 0xc: { const uint32_t t = Rn ^ Rm; setT(!(t & 0xff000000u) || !(t & 0xff0000u) || !(t & 0xff00u) || !(t & 0xffu)); return true; }
        case 0xd: Rn = (Rn >> 16) | (Rm << 16); return true;                                    // xtrct
        case 0xe: macl = (uint32_t)(uint16_t)Rn * (uint32_t)(uint16_t)Rm; return true;
        case 0xf: macl = (uint32_t)(s16(Rn) * s16(Rm)); return true;
        }
        break;
    case 0x3:
        switch (op & 0xf) {
        case 0x0: setT(Rn == Rm); return true;
        case 0x2: setT(Rn >= Rm); return true;
        case 0x3: setT((int32_t)Rn >= (int32_t)Rm); return true;
        case 0x4: {                                                                             // div1
            const uint32_t oldq = (sr >> 8) & 1, M = (sr >> 9) & 1;
            uint32_t q = Rn >> 31;
            const uint32_t tmp2 = Rm;
            Rn = (Rn << 1) | T();
            uint32_t tmp0 = Rn, tmp1;
            if (oldq == M) { Rn -= tmp2; tmp1 = Rn > tmp0; } else { Rn += tmp2; tmp1 = Rn < tmp0; }
            if (oldq == 0) q = M == 0 ? (q ? !tmp1 : tmp1) : (q ? tmp1 : !tmp1);
            else q = M == 0 ? (q ? !tmp1 : tmp1) : (q ? tmp1 : !tmp1);
            sr = (sr & ~0x100u) | (q << 8);
            setT(q == M);
            return true;
        }
        case 0x5: { const uint64_t p = (uint64_t)Rn * (uint64_t)Rm; mach = (uint32_t)(p >> 32); macl = (uint32_t)p; return true; }
        case 0x6: setT(Rn > Rm); return true;
        case 0x7: setT((int32_t)Rn > (int32_t)Rm); return true;
        case 0x8: Rn -= Rm; return true;
        case 0xa: { const uint32_t t1 = Rn - Rm, t0 = Rn; Rn = t1 - T(); setT(t0 < t1 || t1 < Rn); return true; }
        case 0xb: { const int64_t d = (int64_t)(int32_t)Rn - (int32_t)Rm; Rn = (uint32_t)d; setT(d != (int32_t)d); return true; }
        case 0xc: Rn += Rm; return true;
        case 0xd: { const int64_t p = (int64_t)(int32_t)Rn * (int32_t)Rm; mach = (uint32_t)((uint64_t)p >> 32); macl = (uint32_t)p; return true; }
        case 0xe: { const uint32_t t1 = Rn + Rm, t0 = Rn; Rn = t1 + T(); setT(t0 > t1 || t1 > Rn); return true; }
        case 0xf: { const int64_t s = (int64_t)(int32_t)Rn + (int32_t)Rm; Rn = (uint32_t)s; setT(s != (int32_t)s); return true; }
        }
        break;
    case 0x4:
        switch (op & 0xff) {
        case 0x00: setT(Rn >> 31); Rn <<= 1; return true;
        case 0x01: setT(Rn & 1); Rn >>= 1; return true;
        case 0x02: wr32(Rn - 4, mach); Rn -= 4; return true;
        case 0x03: wr32(Rn - 4, sr); Rn -= 4; return true;
        case 0x04: setT(Rn >> 31); Rn = (Rn << 1) | (Rn >> 31); return true;
        case 0x05: setT(Rn & 1); Rn = (Rn >> 1) | (Rn << 31); return true;
        case 0x06: mach = rd32(Rn); Rn += 4; return true;
        case 0x07: sr = rd32(Rn); Rn += 4; return true;
        case 0x08: Rn <<= 2; return true;
        case 0x09: Rn >>= 2; return true;
        case 0x0a: mach = Rn; return true;
        case 0x0b: { const uint32_t t = Rn; pr = pc + 4; return delayed(t); }                  // jsr
        case 0x0e: sr = Rn; return true;
        case 0x10: Rn -= 1; setT(Rn == 0); return true;
        case 0x11: setT((int32_t)Rn >= 0); return true;
        case 0x12: wr32(Rn - 4, macl); Rn -= 4; return true;
        case 0x13: wr32(Rn - 4, gbr); Rn -= 4; return true;
        case 0x15: setT((int32_t)Rn > 0); return true;
        case 0x16: macl = rd32(Rn); Rn += 4; return true;
        case 0x17: gbr = rd32(Rn); Rn += 4; return true;
        case 0x18: Rn <<= 8; return true;
        case 0x19: Rn >>= 8; return true;
        case 0x1a: macl = Rn; return true;
        case 0x1b: { const uint8_t b = rd8(Rn); setT(b == 0); wr8(Rn, b | 0x80); return true; }
        case 0x1e: gbr = Rn; return true;
        case 0x20: setT(Rn >> 31); Rn <<= 1; return true;
        case 0x21: setT(Rn & 1); Rn = (uint32_t)((int32_t)Rn >> 1); return true;
        case 0x22: wr32(Rn - 4, pr); Rn -= 4; return true;
        case 0x24: { const uint32_t t = T(); setT(Rn >> 31); Rn = (Rn << 1) | t; return true; }
        case 0x25: { const uint32_t t = T(); setT(Rn & 1); Rn = (Rn >> 1) | (t << 31); return true; }
        case 0x26: pr = rd32(Rn); Rn += 4; return true;
        case 0x27: vbr = rd32(Rn); Rn += 4; return true;
        case 0x28: Rn <<= 16; return true;
        case 0x29: Rn >>= 16; return true;
        case 0x2a: pr = Rn; return true;
        case 0x2b: { const uint32_t t = Rn; return delayed(t); }                               // jmp
        case 0x2e: vbr = Rn; return true;
        case 0x52: wr32(Rn - 4, fpul); Rn -= 4; return true;
        case 0x56: fpul = rd32(Rn); Rn += 4; return true;
        case 0x5a: fpul = Rn; return true;
        case 0x62: wr32(Rn - 4, fpscr); Rn -= 4; return true;
        case 0x66: { const uint32_t v = rd32(Rn); Rn += 4; setFpscr(v); return true; }
        case 0x6a: setFpscr(Rn); return true;
        case 0xfa: dbr = Rn; return true;
        }
        switch (op & 0xf) {
        case 0xc: {                                                                             // shad
            const int32_t s = (int32_t)Rm;
            if (s >= 0) Rn <<= (s & 31);
            else if ((s & 31) == 0) Rn = (int32_t)Rn < 0 ? 0xffffffffu : 0;
            else Rn = (uint32_t)((int32_t)Rn >> ((~s & 31) + 1));
            return true;
        }
        case 0xd: {                                                                             // shld
            const int32_t s = (int32_t)Rm;
            if (s >= 0) Rn <<= (s & 31);
            else if ((s & 31) == 0) Rn = 0;
            else Rn >>= ((~s & 31) + 1);
            return true;
        }
        case 0xf: {                                                                             // mac.w @Rm+,@Rn+
            const int32_t a = s16(rd16(Rn)), b = s16(rd16(r[m]));
            Rn += 2; r[m] += 2;
            if (sr & 2) {
                int64_t acc = (int64_t)(int32_t)macl + (int64_t)a * b;
                if (acc > 0x7fffffff) { acc = 0x7fffffff; mach |= 1; } else if (acc < -0x7fffffffLL - 1) { acc = -0x7fffffffLL - 1; mach |= 1; }
                macl = (uint32_t)acc;
            } else {
                int64_t acc = (int64_t)(((uint64_t)mach << 32) | macl) + (int64_t)a * b;
                mach = (uint32_t)((uint64_t)acc >> 32); macl = (uint32_t)acc;
            }
            return true;
        }
        case 0xe:
            if (m >= 8) { rbank[m & 7] = Rn; return true; }
            break;
        }
        break;
    case 0x5: Rn = rd32(Rm + (op & 0xf) * 4); return true;
    case 0x6:
        switch (op & 0xf) {
        case 0x0: Rn = (uint32_t)s8(rd8(Rm)); return true;
        case 0x1: Rn = (uint32_t)s16(rd16(Rm)); return true;
        case 0x2: Rn = rd32(Rm); return true;
        case 0x3: Rn = Rm; return true;
        case 0x4: { const uint32_t v = (uint32_t)s8(rd8(Rm)); if (n != m) r[m] += 1; Rn = v; return true; }
        case 0x5: { const uint32_t v = (uint32_t)s16(rd16(Rm)); if (n != m) r[m] += 2; Rn = v; return true; }
        case 0x6: { const uint32_t v = rd32(Rm); if (n != m) r[m] += 4; Rn = v; return true; }
        case 0x7: Rn = ~Rm; return true;
        case 0x8: Rn = (Rm & 0xffff0000u) | ((Rm & 0xff) << 8) | ((Rm >> 8) & 0xff); return true;
        case 0x9: Rn = (Rm << 16) | (Rm >> 16); return true;
        case 0xa: { const uint32_t t = 0 - Rm; Rn = t - T(); setT(0 < t || t < Rn); return true; }
        case 0xb: Rn = 0 - Rm; return true;
        case 0xc: Rn = Rm & 0xff; return true;
        case 0xd: Rn = Rm & 0xffff; return true;
        case 0xe: Rn = (uint32_t)s8(Rm); return true;
        case 0xf: Rn = (uint32_t)s16(Rm); return true;
        }
        break;
    case 0x7: Rn += (uint32_t)s8(op); return true;
    case 0x8: {
        const int32_t d8 = s8(op);
        switch (n) {
        case 0x0: wr8(r[m] + (op & 0xf), (uint8_t)r[0]); return true;
        case 0x1: wr16(r[m] + (op & 0xf) * 2, (uint16_t)r[0]); return true;
        case 0x4: r[0] = (uint32_t)s8(rd8(r[m] + (op & 0xf))); return true;
        case 0x5: r[0] = (uint32_t)s16(rd16(r[m] + (op & 0xf) * 2)); return true;
        case 0x8: setT(r[0] == (uint32_t)d8); return true;
        case 0x9: if (slot) return fault("bt in slot", pc); if (T()) next = pc + 4 + d8 * 2; return true;
        case 0xb: if (slot) return fault("bf in slot", pc); if (!T()) next = pc + 4 + d8 * 2; return true;
        case 0xd: { const bool t = T(); const uint32_t tg = pc + 4 + d8 * 2; if (!delayed(tg)) return false; if (!t) next = pc + 4; return true; }
        case 0xf: { const bool t = T(); const uint32_t tg = pc + 4 + d8 * 2; if (!delayed(tg)) return false; if (t) next = pc + 4; return true; }
        }
        break;
    }
    case 0x9: Rn = (uint32_t)s16(rd16(pc + 4 + (op & 0xff) * 2)); return true;
    case 0xa: { int32_t d = op & 0xfff; if (d & 0x800) d -= 0x1000; return delayed(pc + 4 + d * 2); }
    case 0xb: { int32_t d = op & 0xfff; if (d & 0x800) d -= 0x1000; pr = pc + 4; return delayed(pc + 4 + d * 2); }
    case 0xc: {
        const uint32_t d = op & 0xff;
        switch (n) {
        case 0x0: wr8(gbr + d, (uint8_t)r[0]); return true;
        case 0x1: wr16(gbr + d * 2, (uint16_t)r[0]); return true;
        case 0x2: wr32(gbr + d * 4, r[0]); return true;
        case 0x4: r[0] = (uint32_t)s8(rd8(gbr + d)); return true;
        case 0x5: r[0] = (uint32_t)s16(rd16(gbr + d * 2)); return true;
        case 0x6: r[0] = rd32(gbr + d * 4); return true;
        case 0x7: r[0] = ((pc & ~3u) + 4) + d * 4; return true;                                // mova
        case 0x8: setT((r[0] & d) == 0); return true;
        case 0x9: r[0] &= d; return true;
        case 0xa: r[0] ^= d; return true;
        case 0xb: r[0] |= d; return true;
        case 0xc: setT((rd8(gbr + r[0]) & d) == 0); return true;
        case 0xd: wr8(gbr + r[0], rd8(gbr + r[0]) & d); return true;
        case 0xe: wr8(gbr + r[0], rd8(gbr + r[0]) ^ d); return true;
        case 0xf: wr8(gbr + r[0], rd8(gbr + r[0]) | d); return true;
        }
        break;
    }
    case 0xd: Rn = rd32(((pc & ~3u) + 4) + (op & 0xff) * 4); return true;
    case 0xe: Rn = (uint32_t)s8(op); return true;
    case 0xf: {
        const bool PR = fpscr & (1u << 19), SZ = fpscr & (1u << 20);
        // SZ = 1 pair access: n even → DR(n) = FR(n):FR(n+1); n odd → XD(n−1) = XF(n−1):XF(n).
        auto pairRef = [&](int k, int half) -> uint32_t& { return (k & 1) ? XF((k & ~1) + half) : FR(k + half); };
        auto loadP = [&](int k, uint32_t a) { const uint32_t lo = rd32(a), hi = rd32(a + 4); pairRef(k, 1) = lo; pairRef(k, 0) = hi; };
        auto storeP = [&](int k, uint32_t a) { wr32(a, pairRef(k, 1)); wr32(a + 4, pairRef(k, 0)); };
        switch (op & 0xf) {
        case 0x0: if (PR) setD(n, D(n) + D(m)); else setF(n, F(n) + F(m)); return true;
        case 0x1: if (PR) setD(n, D(n) - D(m)); else setF(n, F(n) - F(m)); return true;
        case 0x2: if (PR) setD(n, D(n) * D(m)); else setF(n, F(n) * F(m)); return true;
        case 0x3: if (PR) setD(n, D(n) / D(m)); else setF(n, F(n) / F(m)); return true;
        case 0x4: setT(PR ? D(n) == D(m) : F(n) == F(m)); return true;
        case 0x5: setT(PR ? D(n) > D(m) : F(n) > F(m)); return true;
        case 0x6: if (SZ) loadP(n, Rm + r[0]); else FR(n) = rd32(Rm + r[0]); return true;
        case 0x7: if (SZ) storeP(m, Rn + r[0]); else wr32(Rn + r[0], FR(m)); return true;
        case 0x8: if (SZ) loadP(n, Rm); else FR(n) = rd32(Rm); return true;
        case 0x9: if (SZ) { loadP(n, Rm); r[m] += 8; } else { FR(n) = rd32(Rm); r[m] += 4; } return true;
        case 0xa: if (SZ) storeP(m, Rn); else wr32(Rn, FR(m)); return true;
        case 0xb: if (SZ) { Rn -= 8; storeP(m, Rn); } else { Rn -= 4; wr32(Rn, FR(m)); } return true;
        case 0xc: if (SZ) { pairRef(n, 0) = pairRef(m, 0); pairRef(n, 1) = pairRef(m, 1); } else FR(n) = FR(m); return true;
        case 0xe: { if (PR) break; const float t = F(0) * F(m); setF(n, t + F(n)); return true; }   // fmac
        case 0xd:
            switch (m) {
            case 0x0: FR(n) = fpul; return true;                                                  // fsts
            case 0x1: fpul = FR(n); return true;                                                  // flds
            case 0x2: if (PR) setD(n, (double)(int32_t)fpul); else setF(n, (float)(int32_t)fpul); return true;
            case 0x3: fpul = (uint32_t)ftrcF(PR ? D(n) : (double)F(n)); return true;
            case 0x4: if (PR) FR(n & ~1) ^= 0x80000000u; else FR(n) ^= 0x80000000u; return true;
            case 0x5: if (PR) FR(n & ~1) &= 0x7fffffffu; else FR(n) &= 0x7fffffffu; return true;
            case 0x6: if (PR) setD(n, std::sqrt(D(n))); else setF(n, std::sqrt(F(n))); return true;
            case 0x8: if (PR) break; FR(n) = 0; return true;
            case 0x9: if (PR) break; FR(n) = 0x3f800000u; return true;
            case 0xa: if (!PR) break; setD(n, (double)asF(fpul)); return true;                   // fcnvsd
            case 0xb: if (!PR) break; fpul = asU((float)D(n)); return true;                       // fcnvds
            case 0xe: {                                                                           // fipr FVm,FVn
                if (PR) break;
                const int vn = n & 0xc, vm = (n & 3) << 2;
                double s = 0;
                for (int i = 0; i < 4; ++i) s += (double)F(vm + i) * (double)F(vn + i);
                setF(vn + 3, (float)s);
                return true;
            }
            case 0xf:
                if (op == 0xfbfd) { setFpscr(fpscr ^ (1u << 21)); return true; }                 // frchg
                if (op == 0xf3fd) { setFpscr(fpscr ^ (1u << 20)); return true; }                 // fschg
                if ((op & 0x03ff) == 0x01fd && !PR) {                                             // ftrv XMTRX,FVn
                    const int vn = n & 0xc;
                    double v[4], o[4];
                    for (int i = 0; i < 4; ++i) v[i] = F(vn + i);
                    for (int i = 0; i < 4; ++i) { o[i] = 0; for (int j = 0; j < 4; ++j) o[i] += (double)asF(XF(i + 4 * j)) * v[j]; }
                    for (int i = 0; i < 4; ++i) setF(vn + i, (float)o[i]);
                    return true;
                }
                break;
            }
            break;
        }
        break;
    }
    }
    char b[64];
    std::snprintf(b, sizeof b, "unsupported opcode %04x", op);
    return fault(b, pc);
}
