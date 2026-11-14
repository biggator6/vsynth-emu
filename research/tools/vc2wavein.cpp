// vc2run <rom.4> wavein <rom.2> <index> <bank>: what rom.4's wave reader puts in its store for each VC-2 sample
// (research/VC2_VOCAL_DESIGNER.md, "The wave reader, run").  rom.4's start-up load (vc2bankload.h) with its wave reader
// (0x893474d0) run, a hook on its first instruction only taking the path and id it is given; the store set up first by
// its own routine, and the OS's allocator and the kernel's file-name call answered.  Then each sample the store holds
// against the plugin's reading of the same stream (vc2wavecmp.h), tallied.  Fails on a fault, a sample not held or not
// read, a field that differs, a sample that differs from whole blocks decoded, or a band wave whose bands differ.
#include "XtFirmware.h"
#include "vc2bankload.h"
#include "XtMultiBandPlayer.h"
#include "vc2wavecmp.h"
#include <algorithm>
#include <cstdio>
#include <map>
#include <string>

namespace vc2 {

namespace {

constexpr uint32_t kHeld = 0x89351840;   // [FW] the store's (id) → 1 when it holds the wave
// [FW] the kernel's (path) → its file's name (0x88008c90): what the reader gives 0x89345be0, which drops a "NNN " or
// "NNN_" number from a name as the card's files have it; answered as the part after the path's last '\'
constexpr uint32_t kFileName = 0x89338c60;
constexpr size_t kPathMost = 64;

void answerFileName(Machine& m) {
    m.cpu.hook(kFileName, [&m](Sh4& c) {
        const std::string path = cString(m, c.r[4], kPathMost);
        const size_t slash = path.rfind('\\');
        c.ret(c.r[4] + (uint32_t)(slash == std::string::npos ? 0 : slash + 1));
    });
}

// [FW] the OS's allocator (size) → memory, its calloc (n, size) and its free (pointer), answered as Machine answers
// operator new: from a region of their own, nothing freed; the store's setup (0x89353ac0 on the library 0x8997c52c: its
// 128 lists cleared, +0 −1, its wave pool 0x8997c51c given 32 MB by 0x89350c10)
constexpr uint32_t kMalloc = 0x89300770, kCalloc = 0x89300910, kFree = 0x89300840;
constexpr uint32_t kStoreSetUp = 0x89353ac0, kLibraryObject = 0x8997c52c;
constexpr uint32_t kMallocFrom = 0x90000000, kMallocRoom = 0x04000000, kAlign = 16;
constexpr uint64_t kLoadSteps = 4'000'000'000;   // the load with the reader run: about 5.7 million steps a sample

// The region's next free byte; memory there is never reused, so a block is zero when given
struct Bump {
    uint32_t next = kMallocFrom;
    long calls = 0;
    uint32_t take(uint32_t bytes) {
        const uint32_t size = (std::max(bytes, 1u) + kAlign - 1) & ~(kAlign - 1);
        const bool fits = size <= kMallocFrom + kMallocRoom - next;
        const uint32_t at = fits ? next : 0;
        next += fits ? size : 0;
        ++calls;
        return at;
    }
};

void answerMalloc(Machine& m, Bump& b) {
    m.cpu.mapRam(kMallocFrom, kMallocRoom);
    m.cpu.hook(kMalloc, [&b](Sh4& c) { c.ret(b.take(c.r[4])); });
    m.cpu.hook(kCalloc, [&b](Sh4& c) { c.ret(b.take(c.r[4] * c.r[5])); });
    m.cpu.hook(kFree, [](Sh4& c) { c.ret(0); });
}

} // namespace

int wavein(Machine& m, int argc, char** argv) {
    if (argc < 4 || !xtFwOpen(argv[1]) || !xtMbLoadTables(argv[1])) {
        std::fprintf(stderr, "%s <rom.2> <VC-2 Wave.bin> <VC-2 Preset.bin>: rom.2 for the RDAC decoder and bands\n",
                     argv[0]);
        return 2;
    }
    BankFiles files;
    if (!openBankFiles(m, files, argv[2], argv[3]) || !loadBank(m)) return 1;
    StartUp start;
    answerStartUp(m, start);
    answerFileName(m);
    Bump bump;
    answerMalloc(m, bump);
    if (!m.run(kStoreSetUp, {kLibraryObject})) {
        std::printf("wavein: the store's setup %s\n", m.error().c_str());
        return 1;
    }
    std::map<uint32_t, std::string> paths;
    m.cpu.hook(kReadWave, [&m, &paths](Sh4& c) { paths[c.r[5]] = readerPath(m, c); });
    const bool loaded = runStartUpLoad(m, argv[0], start, kLoadSteps);
    std::fprintf(stderr, "the OS's allocator: %ld calls, %#x bytes; the reader reached %zu samples, %llu steps\n",
                 bump.calls, bump.next - kMallocFrom, paths.size(), (unsigned long long)m.cpu.steps());
    if (!loaded) return 1;
    const int samples = m.cpu.rd16(kSampleCount);
    WaveTally t;
    for (uint32_t id = 1; id <= (uint32_t)samples; ++id) {
        const std::optional<Stream> s = paths.count(id) ? streamOf(m, paths[id]) : std::nullopt;
        uint32_t held = 0;
        const uint8_t* stream = s ? files.preset.bytes.data() + (s->place - kBankMapped) : nullptr;
        if (!s || !m.run(kHeld, {id}, &held) || !held || !compareStoredWave(m, id, stream, s->size, t)) {
            std::printf("  sample %u: not held, not found, or not read\n", id);
            ++t.unread;
        }
    }
    sayTally(t, samples);
    return t.failed() ? 1 : 0;
}

} // namespace vc2
