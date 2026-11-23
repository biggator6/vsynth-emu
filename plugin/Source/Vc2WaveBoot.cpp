// The VC-2's waves, loaded by rom.4 itself (Vc2WaveBoot.h, v503).
#include "Vc2WaveBoot.h"
#include "Vc2WaveBootKernel.h"
#include "XtFwMachine.h"
#include <cstdio>
#include <tuple>

namespace {
using Kernel = Vc2WaveBoot::Kernel;
// The machine's RAM below the heap: the stack, the stages' states (the boot keeps them on its own stack) and the
// kernel's folder entry
constexpr uint32_t kRam = 0x8C000000, kRamBytes = 0x100000, kIndexState = 0x8C0F1000, kLoadState = 0x8C0F1100;
// [FW] the image header's heap, its first byte and its last word, which rom.4's allocator (0x89300770) runs on
constexpr uint32_t kHeapFirst = 0x89300038, kHeapLast = 0x8930003c;
constexpr uint32_t kPoolOverhang = 24;   // the bytes past the pool's memory its size law can give it (Vc2WavePool)
constexpr uint32_t kKernelFrom = 0x88000000, kKernelTo = 0x89000000;
constexpr uint64_t kLoadSteps = 3'000'000'000ull;   // the start-up load's: the disk image's takes 610 million

// What a stage gives back that the boot goes on from: anything, not negative, or 1
enum class Goes { any, notNegative, one };
struct Stage {
    const char* what;
    uint32_t at;
    uint32_t arg;   // r4: a routine that takes none ignores it
    Goes goes;
    uint64_t steps;
};
// [FW] the static initializers of what the loaders use that C startup doesn't give
constexpr Stage kInitializers[] = {
    { "the folder listings' initializer", 0x893503a0, 0, Goes::any, xtfw::kCallSteps },
    { "their nodes' heap's initializer", 0x89350860, 0, Goes::any, xtfw::kCallSteps },
    { "the bank's initializer", 0x89350af0, 0, Goes::any, xtfw::kCallSteps },
};
// [FW] 0x8933eea0's stages, in its order (Vc2WaveBoot.h)
constexpr Stage kStages[] = {
    { "the store's set-up", 0x89352520, 0, Goes::one, xtfw::kCallSteps },
    { "the sample bank", 0x89350970, 0x89e0f034, Goes::notNegative, xtfw::kCallSteps },
    { "the index's state", 0x8934ce70, kIndexState, Goes::any, xtfw::kCallSteps },
    { "the index", 0x8934d120, kIndexState, Goes::notNegative, xtfw::kCallSteps },
    { "the project's path", 0x89342cb0, 0x8997ab20, Goes::any, xtfw::kCallSteps },
    { "the link files", 0x89349d00, 0, Goes::notNegative, xtfw::kCallSteps },
    { "the user waves", 0x89349ef0, 0, Goes::notNegative, xtfw::kCallSteps },
    { "the start-up load's state", 0x8934d2a0, kLoadState, Goes::any, xtfw::kCallSteps },
    { "the start-up load", 0x8934d2d0, kLoadState, Goes::notNegative, kLoadSteps },
};

bool fail(std::string* why, const std::string& what) {
    if (why) *why = what;
    return false;
}

std::string hex(uint32_t v) {
    char b[16];
    std::snprintf(b, sizeof b, "0x%08x", v);
    return b;
}

// One stage on the machine: false, said, when it faults, runs past its steps, reaches a kernel entry the load doesn't
// answer, opens a file to write, or gives what the boot doesn't go on from
bool ran(Sh4& c, Kernel& kernel, const Stage& s, std::string* why) {
    uint32_t r0 = 0;
    const bool returned = c.call(s.at, { s.arg }, Vc2WaveBoot::kStack, &r0, s.steps);
    const std::string where = std::string("rom.4's ") + s.what + " (" + hex(s.at) + ")";
    if (!returned && c.pc >= kKernelFrom && c.pc < kKernelTo)
        return fail(why, where + " called the kernel at " + hex(c.pc) + ", which the load doesn't answer");
    if (!returned) return fail(why, where + ": " + c.error());
    if (!kernel.refused.empty()) return fail(why, where + " asked for " + kernel.refused);
    const bool goes[] = { true, (int32_t)r0 >= 0, r0 == 1 };
    return goes[(int)s.goes] || fail(why, where + " gave " + std::to_string((int32_t)r0));
}

// Each of `stages` in order, stopping at the first that fails
template <size_t N>
bool ranAll(Sh4& c, Kernel& kernel, const Stage (&stages)[N], std::string* why) {
    for (const Stage& s : stages)
        if (!ran(c, kernel, s, why)) return false;
    return true;
}

bool unstarted(std::string* why) { return fail(why, "rom.4 isn't started"); }
} // namespace

uint8_t Vc2LoadedWaves::byte(uint32_t at) const {
    const uint8_t* data = sets.dataAt(at);
    return data ? *data : pool.byte(at);
}

Vc2WaveBoot::Vc2WaveBoot(const std::string& rom4, const Vc2BootFiles& files)
    : rom4_(rom4), kernel_(std::make_unique<Kernel>(files)) {}

Vc2WaveBoot::~Vc2WaveBoot() = default;

bool Vc2WaveBoot::answers(uint32_t at) { return Kernel::answers(at); }

bool Vc2WaveBoot::start(std::string* why) {
    cpu_ = std::make_unique<Sh4>();
    Sh4& c = *cpu_;
    if (!c.loadRom(rom4_.c_str())) return fail(why, "rom.4 can't be read: " + c.error());
    const uint32_t first = c.rd32(kHeapFirst), last = c.rd32(kHeapLast);
    if (first < kRam + kRamBytes || last < first || last >= Kernel::kMappedFrom)
        return fail(why, "not rom.4: its heap isn't where rom.4's is");
    c.mapRam(kRam, kRamBytes);
    c.mapRam(first, last + 4 - first);
    kernel_->serve(c);
    return true;
}

bool Vc2WaveBoot::initialize(std::string* why) {
    return cpu_ ? ranAll(*cpu_, *kernel_, kInitializers, why) : unstarted(why);
}

bool Vc2WaveBoot::load(std::string* why) { return cpu_ ? ranAll(*cpu_, *kernel_, kStages, why) : unstarted(why); }

void Vc2WaveBoot::take(Vc2LoadedWaves& out) {
    Sh4& c = *cpu_;
    const std::tuple<uint32_t, void*, size_t> words[] = {
        { Vc2WaveStore::kAt, &out.store.words, sizeof out.store.words },
        { Vc2WavePool::kAt, &out.pool.words, sizeof out.pool.words },
        { Vc2WaveSets::kCountsAt, &out.sets.counts, sizeof out.sets.counts },
        { Vc2WaveSets::kTablesAt, &out.sets.tables, sizeof out.sets.tables },
        { Vc2WaveSets::kBuffersAt, &out.sets.buffers, sizeof out.sets.buffers },
        { Vc2WaveSets::kVelocityDataAt, out.sets.data.velocity.data(), out.sets.data.velocity.size() },
        { Vc2WaveSets::kKeyDataAt, out.sets.data.key.data(), out.sets.data.key.size() },
    };
    for (const auto& [at, into, bytes] : words) c.read(at, into, bytes);
    out.pool.memory.assign(out.pool.words.base == 0 ? 0 : out.pool.words.size + kPoolOverhang, 0);
    c.read(out.pool.words.base, out.pool.memory.data(), out.pool.memory.size());
}

bool vc2LoadWaves(const std::string& rom4, const Vc2BootFiles& files, Vc2LoadedWaves& out, std::string* why) {
    Vc2WaveBoot boot(rom4, files);
    if (!boot.start(why) || !boot.initialize(why) || !boot.load(why)) return false;
    boot.take(out);
    return true;
}
