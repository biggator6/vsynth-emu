// vc2run <rom.4> bank <index> <bank>: how rom.4's store comes to hold a VC-2 sample (research/VC2_VOCAL_DESIGNER.md,
// "The sample bank, loaded").  rom.4's start-up load (vc2bankload.h) with the store's "held" (0x89351840) answered from
// what has been loaded and the wave reader (0x893474d0(path, id, 0)) hooked to take what it is given.  Then for each
// sample rom.4's find by name (0x89350a50(bank, path + 7, &size), as the reader calls it): its path, id, place in the
// bank's file, size and kind byte (the index reader's), in `wlflib.py samples VC2`'s form, so the two diff to nothing.
// Fails on a fault, a bank or load that fails, or a sample not loaded or not found.
#include "vc2bankload.h"
#include <cstdio>
#include <map>
#include <string>

namespace vc2 {

namespace {

constexpr uint32_t kHeld = 0x89351840;   // [FW]
constexpr uint32_t kReaderOut = kScratch + 0x24000;
constexpr uint32_t kVpfl = 0x6c667076;   // "vpfl" read as a little-endian word

// What the start-up load gave the wave reader: each id's path, and how many calls
struct Loads {
    std::map<uint32_t, std::string> path;
    long calls = 0;
};

// The store's "held" from what has been loaded, and the reader taking its path and id
void takeReader(Machine& m, Loads& loads) {
    m.cpu.hook(kHeld, [&loads](Sh4& c) { c.ret(loads.path.count(c.r[4]) ? 1 : 0); });
    m.cpu.hook(kReadWave, [&m, &loads](Sh4& c) {
        loads.path[c.r[5]] = readerPath(m, c);
        ++loads.calls;
        c.ret(0);
    });
}

// One sample by rom.4's find, said in wlflib.py's form; false when it isn't loaded or found
bool saySample(Machine& m, const Loads& loads, uint32_t id, uint8_t kind) {
    const auto at = loads.path.find(id);
    if (at == loads.path.end()) {
        std::printf("sample %3u not loaded\n", id);
        return false;
    }
    const std::optional<Stream> s = streamOf(m, at->second);
    if (!s) {
        std::printf("sample %3u %s not found\n", id, at->second.c_str());
        return false;
    }
    const char* tail = m.cpu.rd32(s->place) == kVpfl ? "" : " (no vpfl)";
    std::printf("sample %3u %-34s at +%#x, %u bytes, kind %u%s\n", id, at->second.c_str(), s->place - kBankMapped,
                s->size, kind, tail);
    return true;
}

} // namespace

int bank(Machine& m, int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "%s <VC-2 Wave.bin> <VC-2 Preset.bin>: wlflib.py get VC2, wlflib.py bank VC2\n", argv[0]);
        return 2;
    }
    BankFiles files;
    if (!openBankFiles(m, files, argv[1], argv[2]) || !loadBank(m)) return 1;
    StartUp start;
    answerStartUp(m, start);
    Loads loads;
    takeReader(m, loads);
    if (!runStartUpLoad(m, argv[0], start)) return 1;
    const int samples = m.cpu.rd16(kSampleCount);
    std::fprintf(stderr, "the reader called %ld times for %zu of %d samples\n", loads.calls, loads.path.size(),
                 samples);
    const std::optional<IndexChoices> choices = indexChoices(m, kReaderOut);
    if (!choices) {
        std::printf("bank: the index's readers %s\n", m.error().c_str());
        return 1;
    }
    bool ok = true;
    for (int id = 1; id <= samples; ++id) ok = saySample(m, loads, (uint32_t)id, choices->kind[(size_t)id]) && ok;
    return ok ? 0 : 1;
}

} // namespace vc2
