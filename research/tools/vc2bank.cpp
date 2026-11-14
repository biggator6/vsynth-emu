// vc2run <rom.4> bank <index> <bank>: how rom.4's store comes to hold a VC-2 sample (research/VC2_VOCAL_DESIGNER.md,
// "The sample bank, loaded").  The index (`wlflib.py get VC2`) and the bank (`wlflib.py bank VC2`, "VC-2 Preset.bin")
// answered to rom.4's file calls (vc2index.h), the bank's bytes put in memory and its base given for the kernel's
// 0x89336d60, as the preset area is read as memory.  rom.4's bank object cleared (0x89350af0) and loaded (0x89350970:
// '"TABLE"', the count, 60-byte entries; +0 where the streams start in the file, +4 the file's base); then the index's
// parser, and the start-up load (0x8934d2a0, 0x8934d2d0) with the preset drive opened (0x893369c0, closed 0x893369f0)
// and the display's message (0x8933a890) answered: it takes the Wave Numbers 1…128 (2999 + n) to the wave library
// (0x893520a0) and loads each VC-2 wave it names (0x8934da70: its maps, each map's samples, 0x8934d8d0 a sample by its
// index entry); the library answered as the card's link files have it (Wave/001…058: file k names wave k),
// the store's "held" (0x89351840) from what has been loaded, and the wave reader (0x893474d0(path, id, 0)) hooked to take
// what it is given.  Then for each sample rom.4's find by name (0x89350a50(bank, path + 7, &size), as the reader calls
// it): its path, id, place in the bank's file, size and kind byte (the index reader's), in `wlflib.py samples VC2`'s form,
// so the two diff to nothing.  Fails on a fault, a bank or parse that fails, or a sample not loaded or not found.
#include "vc2index.h"
#include <cstdio>
#include <map>
#include <string>

namespace vc2 {

namespace {

constexpr uint32_t kBank = 0x89e0f034;   // [FW] the bank object, static: +0, +4, then 60-byte entries from +8
constexpr uint32_t kBankClear = 0x89350af0, kBankLoad = 0x89350970, kFind = 0x89350a50;   // [FW]
constexpr uint32_t kDriveOpen = 0x893369c0, kDriveClose = 0x893369f0;   // [FW] (a drive's path) → a handle; (handle)
// [FW] (state): its start (+0 the handle −1, +4 0), then the start-up load: the index opened again, its samples' section
// found (+4), and the Wave Numbers 1…128 loaded
constexpr uint32_t kLoadStart = 0x8934d2a0, kLoadWaves = 0x8934d2d0, kLoadStateBytes = 8;
constexpr uint32_t kShowText = 0x8933a890;   // [FW] the kernel's display message (0x8933a610's), the wave's name shown
constexpr uint32_t kLibrary = 0x893520a0, kHeld = 0x89351840, kReadWave = 0x893474d0;   // [FW]
constexpr const char* kBankPath = "D:\\VC-2 Preset.bin";   // [FW] the bank loader's
constexpr uint32_t kVSynthIds = 2999;                      // [FW] a Wave Number's id less the number
constexpr int kLinkedWaves = 58;   // [COR] Internal.vc2/Wave/001…058, file k naming wave k (wlflib.py)
constexpr uint32_t kNameSkip = 7;   // [FW] the reader looks up path + 7, past "D:\VPW\" (0x893474ec)
constexpr uint32_t kHeaderBytes = 17, kEntryBytes = 60;   // [FW] '"TABLE"', the count, '"DATA"'; an entry
constexpr uint32_t kPathAt = kScratch + 0x26000, kSizeAt = kScratch + 0x26100, kReaderOut = kScratch + 0x24000;
constexpr size_t kPathMost = 64;
constexpr uint32_t kVpfl = 0x6c667076;   // "vpfl" read as a little-endian word
constexpr uint32_t kNone = 0xffffffffu;

// What the start-up load gave the wave reader: each id's path, and how many calls
struct Loads {
    std::map<uint32_t, std::string> path;
    long calls = 0;
    long driveOpens = 0;
};

void answerLoad(Machine& m, Loads& loads) {
    m.cpu.hook(kShowText, [](Sh4& c) { c.ret(0); });
    m.cpu.hook(kDriveOpen, [&loads](Sh4& c) {
        ++loads.driveOpens;
        c.ret(1);
    });
    m.cpu.hook(kDriveClose, [](Sh4& c) { c.ret(0); });
    m.cpu.hook(kLibrary, [](Sh4& c) {
        const auto wave = (int)(c.r[4] - kVSynthIds);
        c.ret(wave >= 1 && wave <= kLinkedWaves ? (uint32_t)wave : kNone);
    });
    m.cpu.hook(kHeld, [&loads](Sh4& c) { c.ret(loads.path.count(c.r[4]) ? 1 : 0); });
    m.cpu.hook(kReadWave, [&m, &loads](Sh4& c) {
        loads.path[c.r[5]] = cString(m, c.r[4], kPathMost);
        ++loads.calls;
        c.ret(0);
    });
}

// One sample by rom.4's find, said in wlflib.py's form; false when it isn't loaded or found
bool saySample(Machine& m, const Loads& loads, uint32_t id, uint8_t kind, uint32_t base) {
    const auto at = loads.path.find(id);
    if (at == loads.path.end()) {
        std::printf("sample %3u not loaded\n", id);
        return false;
    }
    m.cpu.write(kPathAt, at->second.c_str(), at->second.size() + 1);
    uint32_t place = 0;
    if (!m.run(kFind, {kBank, kPathAt + kNameSkip, kSizeAt}, &place) || place == 0) {
        std::printf("sample %3u %s not found\n", id, at->second.c_str());
        return false;
    }
    const char* tail = m.cpu.rd32(place) == kVpfl ? "" : " (no vpfl)";
    std::printf("sample %3u %-34s at +%#x, %u bytes, kind %u%s\n", id, at->second.c_str(), place - base,
                m.cpu.rd32(kSizeAt), kind, tail);
    return true;
}

// The bank cleared and loaded by rom.4's routines; false, said, when the loader doesn't give 1
bool loadBank(Machine& m, uint32_t base) {
    uint32_t loaded = 0;
    const bool cleared = m.run(kBankClear, {});
    if (!cleared || !m.run(kBankLoad, {kBank}, &loaded) || loaded != 1) {
        std::printf("bank: the bank loader gave %d %s\n", (int32_t)loaded, m.error().c_str());
        return false;
    }
    const uint32_t streams = m.cpu.rd32(kBank);
    std::fprintf(stderr, "the bank: +0 %#x (17 + 60 × %u), +4 %#x (the file's base %#x)\n", streams,
                 (streams - kHeaderBytes) / kEntryBytes, m.cpu.rd32(kBank + 4), base);
    return true;
}

} // namespace

int bank(Machine& m, int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "%s <VC-2 Wave.bin> <VC-2 Preset.bin>: wlflib.py get VC2, wlflib.py bank VC2\n", argv[0]);
        return 2;
    }
    FileImage index{fileContents(argv[1])}, preset{fileContents(argv[2])};
    preset.mapped = m.alloc((uint32_t)preset.bytes.size());
    if (index.bytes.empty() || preset.bytes.empty() || preset.mapped == 0) {
        std::printf("bank: no index, no bank, or no room for it\n");
        return 1;
    }
    m.cpu.write(preset.mapped, preset.bytes.data(), preset.bytes.size());
    answerKernel(m);
    hookFiles(m, {{kIndexPath, &index}, {kBankPath, &preset}});
    if (!loadBank(m, preset.mapped)) return 1;
    Loads loads;
    answerLoad(m, loads);
    const std::optional<int32_t> parsed = runParser(m, argv[0]);
    if (!parsed) return 1;
    const uint32_t state = m.alloc(kLoadStateBytes);
    uint32_t waves = 0;
    if (!m.run(kLoadStart, {state}) || !m.run(kLoadWaves, {state}, &waves)) {
        std::printf("bank: the start-up load %s\n", m.error().c_str());
        return 1;
    }
    const int samples = m.cpu.rd16(kSampleCount);
    std::fprintf(stderr, "the parser: %d; the start-up load: %d, the drive opened %ld times, ", *parsed, (int32_t)waves,
                 loads.driveOpens);
    std::fprintf(stderr, "the reader called %ld times for %zu of %d samples\n", loads.calls, loads.path.size(),
                 samples);
    const std::optional<IndexChoices> choices = indexChoices(m, kReaderOut);
    if (!choices) {
        std::printf("bank: the index's readers %s\n", m.error().c_str());
        return 1;
    }
    bool ok = *parsed == 1 && waves == 1;
    for (int id = 1; id <= samples; ++id)
        ok = saySample(m, loads, (uint32_t)id, choices->kind[(size_t)id], preset.mapped) && ok;
    return ok ? 0 : 1;
}

} // namespace vc2
