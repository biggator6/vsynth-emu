#pragma once
// The VC-2's waves, loaded by rom.4 itself (v503): its own boot loaders run in the interpreter, as the XT runs them at
// power-on (0x8933eea0's stages), and what they leave in its memory taken as the port's objects.  The store, its pool,
// the sets and every routine that fills them are rom.4's own code here; only the kernel is answered (Vc2WaveBootKernel.cpp):
// its file and folder calls from a Vc2BootFiles, its string compares, its locks, the display, the boot's key test.
// A kernel entry the table doesn't answer faults the load, which says where.  rom.4's own allocator (0x89300770) runs
// on its heap, the bounds its image's header gives (0x8cc80000 to 0x8ffffffc).
//
// The stages, in the boot's order: the store's set-up (0x89352520, 32 MB for its pool and the sampling record), the
// sample bank (0x89350970 on 0x89e0f034), the index's state and parse (0x8934ce70, 0x8934d120), the project's path
// (0x89342cb0: "C:\VC-2 Project.ini", or "A:\Internal.vc2\"), the Wave folder's link files (0x89349d00: each "NNN
// name.w02" a library record of Wave Number NNN), its user waves (0x89349ef0), the start-up load's state and the load
// of every sample the library's waves use (0x8934d2a0, 0x8934d2d0).  The boot's other stages, the patch lists, the
// sampling object, its mode tests (0x8930f200, 0x8930f1e0, 0x8931a000…, 0x89359a20, 0x89359e60, 0x89341e90), are left
// out: none reads or writes what the waves' loaders do.  Before them, the static initializers of what they use that C
// startup doesn't give: the folder listings (0x893503a0), their nodes' heap (0x89350860), the bank (0x89350af0).
// xt_vc2waves' fourth part runs every one of rom.4's 382 first and finds the same memory, byte for byte.
//
// The disk image's load: 610 million steps, about 2.5 s.  HARDWARE_FIRMWARE.md, "The VC-2's waves, loaded by rom.4
// itself".
#include "Sh4Interp.h"
#include "Vc2BootFiles.h"
#include "Vc2WavePool.h"
#include "Vc2WaveSets.h"
#include <memory>
#include <string>

// What rom.4's loaders leave, as the port's objects: the store's words, the sets, and the pool with its records, chunks
// and blocks.  Itself the wave memory a wave is read from by rom.4's address (what Vc2SetReads::waveMemory gives): the
// sets' data, where their streams lie, and the pool, 0 elsewhere
struct Vc2LoadedWaves : public Vc2WaveMemory {
    Vc2WaveStore store;
    Vc2WaveSets sets;
    Vc2WavePool pool;
    uint8_t byte(uint32_t at) const override;
};

class Vc2WaveBoot {
public:
    // rom.4's machine: its image at `rom4`, the heap and the kernel's answers from `files`, which must outlive this
    Vc2WaveBoot(const std::string& rom4, const Vc2BootFiles& files);
    ~Vc2WaveBoot();
    Vc2WaveBoot(const Vc2WaveBoot&) = delete;
    Vc2WaveBoot& operator=(const Vc2WaveBoot&) = delete;
    // The image read and laid out: false, `why` said, when it can't be
    bool start(std::string* why);
    // The loaders' static initializers, each once: false, said, when one faults or runs past its steps
    bool initialize(std::string* why);
    // The stages: false, said, when one faults, runs past its steps or gives what the boot doesn't go on from
    bool load(std::string* why);
    // What the loaders left, by rom.4's addresses
    void take(Vc2LoadedWaves& out);
    // rom.4's machine after the load, for rom.4's own lookups on the same memory (xt_vc2waves)
    Sh4& cpu() { return *cpu_; }
    // Whether the kernel's answers include rom.4's entry `at` (xt_vc2waves answers the other initializers' calls)
    static bool answers(uint32_t at);
    // The stack the stages run on, for a caller's own calls on the machine
    static constexpr uint32_t kStack = 0x8C0E0000;
    struct Kernel;   // the kernel's answers: Vc2WaveBootKernel.h

private:
    const std::string rom4_;
    std::unique_ptr<Sh4> cpu_;
    std::unique_ptr<Kernel> kernel_;
};

// The whole load: rom.4 at `rom4` on `files`, its waves taken into `out`; false, said, when it fails
bool vc2LoadWaves(const std::string& rom4, const Vc2BootFiles& files, Vc2LoadedWaves& out, std::string* why);
