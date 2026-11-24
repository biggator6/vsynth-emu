#pragma once
// The part's memory by rom.4's address (v342, Vc2Synth whole's third version): every member of Vc2Synth at its own
// address, the whole objects and the tables a field at a time (the table xt_vc2synth's start compares, v314, moved
// here), and a read of any of it by address, which C's reads of the part go through (Vc2SynthReads).  Bytes no region
// holds read 0.  The notes' and update tasks.
#include "Vc2Synth.h"
#include <vector>

// [FW] the vocoder object, its edit words, a tone's tail's place in it
inline constexpr uint32_t kVc2SynthVocoderAt = 0x89999758, kVc2SynthEditAt = 0x8994025c, kVc2ToneTailAt = 0x1af8;

// A region of the part: its rom.4 address, the port's bytes for it, how many, what it is
struct Vc2SynthRegion {
    uint32_t at;
    void* mine;
    size_t bytes;
    const char* what;
};

template <class T>
Vc2SynthRegion vc2SynthRegion(uint32_t at, T& mine, const char* what) {
    return { at, &mine, sizeof mine, what };
}

// Every member's place: the whole objects, the tables a field at a time, the mixes' rings, each pool tone's head and
// tail
std::vector<Vc2SynthRegion> vc2SynthRegions(Vc2Synth& s);

class Vc2SynthMemory {
public:
    explicit Vc2SynthMemory(Vc2Synth& s);
    // `bytes` of the part at rom.4's address `at`, each byte its region's, 0 where none holds it (v343: counted)
    void read(uint32_t at, void* to, size_t bytes);
    // v343: `bytes` into the part at `at`, each byte into its region's, none where none holds it (counted)
    void write(uint32_t at, const void* from, size_t bytes);
    // How many bytes read or written so far that no region holds
    long unheld() const { return unheld_; }

private:
    // The byte of the part at `at`, none (counted) where no region holds it
    uint8_t* byteAt(uint32_t at);
    const Vc2SynthRegion* holding(uint32_t at) const;
    std::vector<Vc2SynthRegion> regions_;   // by address
    long unheld_ = 0;
};
