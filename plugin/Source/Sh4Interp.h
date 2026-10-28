#pragma once
// A small SH-4 (little-endian) interpreter for running the V-Synth XT's own
// load-time and encode-time routines from the user's rom.2 image: the code
// is executed, never copied.  Integer core, FPU (single and double precision,
// both banks, SZ = 1 pair moves, FIPR / FTRV) with the rounding mode taken
// from FPSCR.RM; no MMU, caches, exceptions or peripherals.  Firmware calls
// that need the OS (malloc, UI) are served by C++ hooks.
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

class Sh4 {
public:
    Sh4();
    ~Sh4();

    // Maps rom.2 at 0x89300000 and leaves memory as the C startup does
    // (.data copied from its ROM image, .bss zeroed).
    bool loadRom(const char* path);
    // RAM for the caller's buffers, heap and stack (virtual P1 addresses).
    void mapRam(uint32_t addr, uint32_t size);

    // Memory access (addresses are virtual; P1/P2 alias the same memory).
    uint8_t* ptr(uint32_t addr, uint32_t len);          // nullptr if unmapped
    uint32_t rd32(uint32_t a);
    uint16_t rd16(uint32_t a);
    uint8_t rd8(uint32_t a);
    void wr32(uint32_t a, uint32_t v);
    void wr16(uint32_t a, uint16_t v);
    void wr8(uint32_t a, uint8_t v);
    void write(uint32_t a, const void* src, size_t n);
    void read(uint32_t a, void* dst, size_t n);

    // Registers
    uint32_t r[16] = {};
    uint32_t pc = 0, pr = 0, sr = 0, gbr = 0, vbr = 0, ssr = 0, spc = 0, dbr = 0, rbank[8] = {};
    uint32_t mach = 0, macl = 0, fpul = 0, fpscr = 0x00040001;
    uint32_t fb[2][16] = {};                            // float banks (FPSCR.FR selects)

    // Hooks: called when execution reaches addr (before the instruction).
    // A hook usually sets r[0] and returns to pr (see ret()).
    void hook(uint32_t addr, std::function<void(Sh4&)> fn);
    void ret(uint32_t v) { r[0] = v; pc = pr; }

    // Call a firmware routine: r4..r7 = args, stack at sp, returns r0.  Runs
    // until it returns to an internal sentinel; false on a fault or after
    // maxSteps instructions (error() says why).
    bool call(uint32_t addr, std::initializer_list<uint32_t> args, uint32_t sp, uint32_t* r0 = nullptr,
              uint64_t maxSteps = 20000000000ull);
    const std::string& error() const { return err; }
    uint64_t steps() const { return nsteps; }

    // Round to nearest regardless of FPSCR.RM (Ghidra's P-code emulator does).
    bool forceNearest = false;

private:
    struct Region { uint32_t base, size; std::vector<uint8_t> mem; };
    std::vector<Region*> regions;
    std::vector<uint8_t*> page;                         // 4 KB pages over the 29-bit physical space
    std::unordered_map<uint32_t, std::function<void(Sh4&)>> hooks;
    std::vector<uint8_t> hookPage;
    std::string err;
    uint64_t nsteps = 0;
    int hostRound = -1;

    void addRegion(uint32_t base, uint32_t size);
    uint8_t* at(uint32_t a, uint32_t n);
    void setFpscr(uint32_t v);
    void syncRound();
    bool step();                                        // one instruction (plus its delay slot)
    bool exec(uint16_t op, bool slot, uint32_t& next);
    bool fault(const char* what, uint32_t addr);

    // FPU helpers
    uint32_t& FR(int n) { return fb[(fpscr >> 21) & 1][n]; }
    uint32_t& XF(int n) { return fb[((fpscr >> 21) & 1) ^ 1][n]; }
    float F(int n);
    void setF(int n, float v);
    double D(int n);
    void setD(int n, double v);
};
