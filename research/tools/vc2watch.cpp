// A read-watch on rom.4 in the interpreter (vc2watch.h).
#include "vc2watch.h"
#include <array>
#include <optional>

namespace vc2 {

namespace {

// How a load forms its address: from Rm (bits 4-7), R0 + Rm, Rm + disp (bits 0-3) × scale, GBR + disp (bits 0-7) × scale,
// GBR + R0, or Rn (bits 8-11, lds.l/ldc.l @Rm+'s register)
enum class Base { rm, r0rm, rmDisp, gbrDisp, gbrR0, rn };
struct LoadForm {
    uint16_t mask, match;
    Base base;
    uint32_t scale, width;   // width 0: fmov's, 4 or 8 by FPSCR.SZ
};
constexpr std::array kLoads{
    LoadForm{0xf00f, 0x6000, Base::rm, 0, 1},       LoadForm{0xf00f, 0x6001, Base::rm, 0, 2},
    LoadForm{0xf00f, 0x6002, Base::rm, 0, 4},       LoadForm{0xf00f, 0x6004, Base::rm, 0, 1},
    LoadForm{0xf00f, 0x6005, Base::rm, 0, 2},       LoadForm{0xf00f, 0x6006, Base::rm, 0, 4},
    LoadForm{0xf00f, 0x000c, Base::r0rm, 0, 1},     LoadForm{0xf00f, 0x000d, Base::r0rm, 0, 2},
    LoadForm{0xf00f, 0x000e, Base::r0rm, 0, 4},     LoadForm{0xf000, 0x5000, Base::rmDisp, 4, 4},
    LoadForm{0xff00, 0x8400, Base::rmDisp, 1, 1},   LoadForm{0xff00, 0x8500, Base::rmDisp, 2, 2},
    LoadForm{0xff00, 0xc400, Base::gbrDisp, 1, 1},  LoadForm{0xff00, 0xc500, Base::gbrDisp, 2, 2},
    LoadForm{0xff00, 0xc600, Base::gbrDisp, 4, 4},  LoadForm{0xfc00, 0xcc00, Base::gbrR0, 0, 1},
    LoadForm{0xf00f, 0xf008, Base::rm, 0, 0},       LoadForm{0xf00f, 0xf009, Base::rm, 0, 0},
    LoadForm{0xf00f, 0xf006, Base::r0rm, 0, 0},     LoadForm{0xf00f, 0x4006, Base::rn, 0, 4},
    LoadForm{0xf00f, 0x4007, Base::rn, 0, 4},
};
// Delayed branches: bra, bsr, bt/s, bf/s, rts, rte, braf, bsrf, jmp, jsr
struct Form {
    uint16_t mask, match;
};
constexpr std::array kDelayed{Form{0xf000, 0xa000}, Form{0xf000, 0xb000}, Form{0xff00, 0x8d00}, Form{0xff00, 0x8f00},
                              Form{0xffff, 0x000b}, Form{0xffff, 0x002b}, Form{0xf0ff, 0x0023}, Form{0xf0ff, 0x0003},
                              Form{0xf0ff, 0x402b}, Form{0xf0ff, 0x400b}};
constexpr uint32_t kSizeBit = 1u << 20;   // FPSCR.SZ: fmov moves pairs

std::optional<LoadForm> loadFormOf(uint16_t op) {
    for (const LoadForm& f : kLoads)
        if ((op & f.mask) == f.match) return f;
    return std::nullopt;
}

bool delayedBranch(uint16_t op) {
    for (const Form& f : kDelayed)
        if ((op & f.mask) == f.match) return true;
    return false;
}

uint32_t addressOf(const Sh4& c, uint16_t op, const LoadForm& f) {
    const uint32_t rm = c.r[op >> 4 & 15];
    switch (f.base) {
        case Base::rm: return rm;
        case Base::r0rm: return c.r[0] + rm;
        case Base::rmDisp: return rm + (op & 15u) * f.scale;
        case Base::gbrDisp: return c.gbr + (op & 0xffu) * f.scale;
        case Base::gbrR0: return c.gbr + c.r[0];
        case Base::rn: return c.r[op >> 8 & 15];
    }
    return 0;
}

} // namespace

ReadWatch::ReadWatch(Machine& m, uint32_t codeFrom, uint32_t codeTo) {
    for (uint32_t pc = codeFrom; pc < codeTo; pc += 2) {
        const uint16_t op = m.cpu.rd16(pc);
        if (!loadFormOf(op) && !delayedBranch(op)) continue;
        m.cpu.hook(pc, [this](Sh4& c) {
            note(c, c.pc);
            if (delayedBranch(c.rd16(c.pc))) note(c, c.pc + 2);
        });
        ++hooked;
    }
}

void ReadWatch::watch(uint32_t start, uint32_t end) {
    from = start;
    to = end;
    seen.clear();
}

void ReadWatch::note(Sh4& c, uint32_t pc) {
    const uint16_t op = c.rd16(pc);
    const std::optional<LoadForm> f = loadFormOf(op);
    if (!f) return;
    const uint32_t at = addressOf(c, op, *f), width = f->width ? f->width : (c.fpscr & kSizeBit ? 8 : 4);
    if (at >= to || at + width <= from) return;
    Seen& s = seen[{pc, (int32_t)(at - from)}];
    s.width = width;
    ++s.times;
}

} // namespace vc2
