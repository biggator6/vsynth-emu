#pragma once
// The VC-2 Vocal Designer's rom.4 in the SH-4 interpreter, for research (research/VC2_VOCAL_DESIGNER.md, "Running rom.4").
// Sh4::loadRom holds rom.2's memory map, so this maps rom.4 by its own header instead, through Sh4's public mapRam and
// write: the image at 0x89300000 (CPU address), .data copied from the ROM just below its start (the source ends where the
// copy begins, Sh4Interp's rule for rom.2), .bss zeroed.  Then RAM for the stack and a heap, laid out as XtFwMachine's,
// and operator new (0x89506df4) answered from that heap.  Never shipped; plugin/Source is unchanged.
#include "Sh4Interp.h"
#include <cstdint>
#include <initializer_list>
#include <string>

namespace vc2 {

// rom.4's sections, read from its header words at file offset 0x1c (CPU addresses)
struct Sections {
    uint32_t bss = 0, bssEnd = 0, data = 0, dataFrom = 0;
};

// The harness memory, as XtFwMachine's (CPU addresses, P1)
inline constexpr uint32_t kRam = 0x8C000000, kRamSize = 0x02000000;
inline constexpr uint32_t kStack = 0x8C0E0000, kScratch = 0x8C100000, kHeap = 0x8D000000;
inline constexpr uint32_t kOperatorNew = 0x89506df4;   // [FW] rom.4: called with a size, as 0x893f0490 does with 0x5a78

// The SH-4's single precision for the models of its routines: round-to-zero (FPSCR.RM) while a scope holds, and FMAC
// as the product rounded, then the sum (two roundings), as Sh4Interp runs them
class RoundToZero {
public:
    RoundToZero();
    ~RoundToZero();
    RoundToZero(const RoundToZero&) = delete;
    RoundToZero& operator=(const RoundToZero&) = delete;
private:
    int saved;
};
float fmac(float a, float b, float acc);

class Machine {
public:
    Sh4 cpu;
    Sections sec;
    explicit Machine(const std::string& rom);
    [[nodiscard]] bool ok() const { return why.empty(); }
    [[nodiscard]] const std::string& error() const { return why; }
    // A call on the harness stack: false, with error() saying why, on a fault or past maxSteps
    [[nodiscard]] bool run(uint32_t addr, std::initializer_list<uint32_t> args, uint32_t* r0 = nullptr,
                           uint64_t maxSteps = 200'000'000);
    uint32_t alloc(uint32_t size);   // zeroed, 16-byte aligned; 0 when the heap is full
    float f32(uint32_t a);
    void setF32(uint32_t a, float f);
private:
    uint32_t heap = kHeap;
    std::string why;
};

} // namespace vc2
