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
    // One call on each: rom.2's routine at a2, rom.4's at a4, the same r4…r7 and fr4; false, said, on a fault in either
    [[nodiscard]] bool call(uint32_t a2, uint32_t a4, std::initializer_list<uint32_t> args, float fr4 = 0);
    struct Pair {
        uint32_t vsynth, vc2;   // a routine in rom.2 and its counterpart in rom.4
    };
    [[nodiscard]] bool call(Pair p, std::initializer_list<uint32_t> args, float fr4 = 0) {
        return call(p.vsynth, p.vc2, args, fr4);
    }
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
private:
    bool ready = false;
};

// A block's side-by-side run as vc2run calls it (`vc2run <rom.4> <name> <rom.2>`): the usage checked, the port's tables
// read from rom.2, the twin made, then `body`, which says how many of its cases differed (−1 on a fault)
int twinRun(Machine& m, int argc, char** argv, bool (*loadTables)(const char*), int (*body)(Twin&));

} // namespace vc2
