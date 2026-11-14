#pragma once
// A read-watch on rom.4 in the interpreter, which has none of its own: every load instruction in a stretch of code hooked,
// and each read it makes inside a window of memory kept by the instruction that made it.  An instruction's hook decodes
// its effective address from the registers as they stand before it runs; a delayed branch's hook decodes its slot too,
// which the interpreter runs with the branch, unhooked.  Loads: mov.b/w/l @Rm, @Rm+, @(R0,Rm), @(disp,Rm), @(disp,GBR),
// the byte operations on @(R0,GBR), fmov.s/d @Rm, @Rm+, @(R0,Rm), lds.l/ldc.l @Rm+.  Not watched: mac.w/mac.l's operands,
// tas.b, and code outside the stretch.  Install it before any hook the run answers with inside the stretch: a later hook
// at the same address replaces the watch's.
#include "vc2runs.h"
#include <cstdint>
#include <map>
#include <utility>

namespace vc2 {

class ReadWatch {
public:
    // Hooks every load and delayed branch in [codeFrom, codeTo); how many in `hooked`
    ReadWatch(Machine& m, uint32_t codeFrom, uint32_t codeTo);
    ReadWatch(const ReadWatch&) = delete;
    ReadWatch& operator=(const ReadWatch&) = delete;
    // The window [from, to) watched from now, what was seen forgotten
    void watch(uint32_t from, uint32_t to);
    // (the instruction's address, the read's first byte less the window's start) → its width in bytes; how many times
    struct Seen {
        uint32_t width = 0;
        long times = 0;
    };
    using Reads = std::map<std::pair<uint32_t, int32_t>, Seen>;
    const Reads& reads() const { return seen; }
    long hooked = 0;

private:
    void note(Sh4& c, uint32_t pc);
    Reads seen;
    uint32_t from = 0, to = 0;
};

} // namespace vc2
