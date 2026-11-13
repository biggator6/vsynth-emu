#pragma once
// rom.2 and rom.4 side by side, for the VC-2 mode's step 1b (research/VC2_MODE_PLAN.md): the same calls on both images'
// routines, a pair by codepairs.py, with the same arguments and memory, and what differs between them after each.  rom.2's
// machine is Sh4::loadRom's, rom.4's vc2rom's; both have the harness RAM at kRam, so a buffer is at one address in both.
#include "vc2rom.h"
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <span>

namespace vc2 {

class Twin {
public:
    Sh4 vsynth;      // rom.2
    Machine& vc2;    // rom.4
    Twin(const char* rom2, Machine& rom4);
    [[nodiscard]] bool ok() const { return ready; }
    // One call on each: rom.2's routine at a2, rom.4's at a4, the same r4…r7, fr4 and fr5; false, said, on a fault in
    // either
    [[nodiscard]] bool call(uint32_t a2, uint32_t a4, std::initializer_list<uint32_t> args, float fr4 = 0,
                            float fr5 = 0);
    struct Pair {
        uint32_t vsynth, vc2;   // a routine in rom.2 and its counterpart in rom.4
    };
    [[nodiscard]] bool call(Pair p, std::initializer_list<uint32_t> args, float fr4 = 0, float fr5 = 0) {
        return call(p.vsynth, p.vc2, args, fr4, fr5);
    }
    // A routine's return compared over many calls: r0, or fr0 for one that returns a float
    enum class Reg { r0, fr0 };
    struct Returns {
        long cases = 0, differ = 0;
    };
    // One call on each image and its return compared, counted in `tally`: false on a fault
    [[nodiscard]] bool returns(Pair p, std::initializer_list<uint32_t> args, Reg reg, Returns& tally, float fr4 = 0,
                               float fr5 = 0);
    // The same hook at a routine's address in each image
    void hook(Pair p, const std::function<void(Sh4&)>& fn);
    void write(uint32_t a, const void* src, size_t n);   // into both
    void writeF32(uint32_t a2, uint32_t a4, float f);    // a global, at each image's own address
    void copyToVc2(uint32_t a, uint32_t n);              // rom.2's machine's bytes at a into rom.4's
    // One call on rom.2's machine alone (what makes an input both are given): false, said, on a fault
    [[nodiscard]] bool callVSynth(uint32_t a2, std::initializer_list<uint32_t> args);
    // An address one image has where the other has its own: rom.2's and rom.4's, from codepairs.py --pools
    struct Moved {
        uint32_t vsynth, vc2;
    };
    // The first word in [a, a + n) (n a multiple of 4) where the two differ, as a byte offset, or −1; a word holding
    // one of `moved`'s addresses in each image is the same
    [[nodiscard]] long differs(uint32_t a, uint32_t n, std::span<const Moved> moved = {});
    // Says where differs() found the objects at `a` part, with each image's word there
    void sayDiffers(uint32_t a, long at);
    // As differs, for an object rom.4 holds at another address than rom.2 (a field inserted before it): the first word in
    // n bytes from a2 in rom.2 and a4 in rom.4 that differs, as a byte offset, or −1
    [[nodiscard]] long differsAt(uint32_t a2, uint32_t a4, uint32_t n);
private:
    bool ready = false;
    long differsMoved(uint32_t a2, uint32_t a4, uint32_t n, std::span<const Moved> moved);
};

// The runs' random inputs: a fixed linear congruential sequence (xt_cosm's), so a run repeats
struct Rng {
    uint32_t s = 12345;
    uint32_t next() {
        s = s * 1103515245u + 12345u;
        return (s >> 8) & 0xffff;
    }
    float in(float lo, float hi) { return lo + (hi - lo) * (float)next() / 65535.0f; }
};

// A block's side-by-side run as vc2run calls it (`vc2run <rom.4> <name> <rom.2>`): the usage checked, the port's tables
// read from rom.2, the twin made, then `body`, which says how many of its cases differed (−1 on a fault)
int twinRun(Machine& m, int argc, char** argv, bool (*loadTables)(const char*), int (*body)(Twin&));
// twinRun's loadTables for a run with no port linked: rom.2 against rom.4 alone
inline bool noPortTables(const char*) {
    return true;
}

} // namespace vc2
