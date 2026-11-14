// rom.4's start-up load of the VC-2's samples in the interpreter (vc2bankload.h).
#include "vc2bankload.h"
#include <cstdio>

namespace vc2 {

namespace {

constexpr uint32_t kBankClear = 0x89350af0, kBankLoad = 0x89350970;   // [FW]
constexpr uint32_t kDriveOpen = 0x893369c0, kDriveClose = 0x893369f0;   // [FW] (a drive's path) → a handle; (handle)
// [FW] (state): its start (+0 the handle −1, +4 0), then the start-up load: the index opened again, its samples' section
// found (+4), and the Wave Numbers 1…128 loaded
constexpr uint32_t kLoadStart = 0x8934d2a0, kLoadWaves = 0x8934d2d0, kLoadStateBytes = 8;
constexpr uint32_t kShowText = 0x8933a890;   // [FW] the kernel's display message (0x8933a610's), the wave's name shown
constexpr uint32_t kLibrary = 0x893520a0;   // [FW] (2999 + the Wave Number) → a VC-2 wave
constexpr const char* kBankPath = "D:\\VC-2 Preset.bin";   // [FW] the bank loader's
constexpr uint32_t kVSynthIds = 2999;                      // [FW] a Wave Number's id less the number
constexpr int kLinkedWaves = 58;   // [COR] Internal.vc2/Wave/001…058, file k naming wave k (wlflib.py)
constexpr uint32_t kHeaderBytes = 17, kEntryBytes = 60;   // [FW] '"TABLE"', the count, '"DATA"'; an entry
constexpr uint32_t kNone = 0xffffffffu;
constexpr uint32_t kFind = 0x89350a50;   // [FW] (bank, name, &size) → the stream's place
constexpr uint32_t kNameSkip = 7;        // [FW] the reader looks up path + 7, past "D:\VPW\" (0x893474ec)
constexpr uint32_t kPathAt = kScratch + 0x26000, kSizeAt = kScratch + 0x26100;
constexpr size_t kPathMost = 64;

} // namespace

bool openBankFiles(Machine& m, BankFiles& f, const char* indexPath, const char* bankPath) {
    f.index.bytes = fileContents(indexPath);
    f.preset.bytes = fileContents(bankPath);
    if (f.index.bytes.empty() || f.preset.bytes.empty() || f.preset.bytes.size() > kBankRoom) {
        std::printf("no index, no bank, or a bank past %#x bytes\n", kBankRoom);
        return false;
    }
    m.cpu.mapRam(kBankMapped, kBankRoom);
    m.cpu.write(kBankMapped, f.preset.bytes.data(), f.preset.bytes.size());
    f.preset.mapped = kBankMapped;
    answerKernel(m);
    hookFiles(m, {{kIndexPath, &f.index}, {kBankPath, &f.preset}});
    return true;
}

bool loadBank(Machine& m) {
    uint32_t loaded = 0;
    const bool cleared = m.run(kBankClear, {});
    if (!cleared || !m.run(kBankLoad, {kBank}, &loaded) || loaded != 1) {
        std::printf("the bank loader gave %d %s\n", (int32_t)loaded, m.error().c_str());
        return false;
    }
    const uint32_t streams = m.cpu.rd32(kBank);
    std::fprintf(stderr, "the bank: +0 %#x (17 + 60 × %u), +4 %#x (the file's base %#x)\n", streams,
                 (streams - kHeaderBytes) / kEntryBytes, m.cpu.rd32(kBank + 4), kBankMapped);
    return true;
}

void answerStartUp(Machine& m, StartUp& s) {
    m.cpu.hook(kShowText, [](Sh4& c) { c.ret(0); });
    m.cpu.hook(kDriveOpen, [&s](Sh4& c) {
        ++s.driveOpens;
        c.ret(1);
    });
    m.cpu.hook(kDriveClose, [](Sh4& c) { c.ret(0); });
    m.cpu.hook(kLibrary, [](Sh4& c) {
        const auto wave = (int)(c.r[4] - kVSynthIds);
        c.ret(wave >= 1 && wave <= kLinkedWaves ? (uint32_t)wave : kNone);
    });
}

bool runStartUpLoad(Machine& m, const char* who, StartUp& s, uint64_t steps) {
    const std::optional<int32_t> parsed = runParser(m, who);
    if (!parsed) return false;
    s.parsed = *parsed;
    const uint32_t state = m.alloc(kLoadStateBytes);
    uint32_t loaded = 0;
    if (!m.run(kLoadStart, {state}) || !m.run(kLoadWaves, {state}, &loaded, steps)) {
        std::printf("%s: the start-up load %s\n", who, m.error().c_str());
        return false;
    }
    s.loaded = (int32_t)loaded;
    std::fprintf(stderr, "the parser: %d; the start-up load: %d, the drive opened %ld times\n", s.parsed, s.loaded,
                 s.driveOpens);
    return s.parsed == 1 && s.loaded == 1;
}

std::string readerPath(Machine& m, const Sh4& c) {
    return cString(m, c.r[4], kPathMost);
}

std::optional<Stream> streamOf(Machine& m, const std::string& path) {
    m.cpu.write(kPathAt, path.c_str(), path.size() + 1);
    uint32_t place = 0;
    if (!m.run(kFind, {kBank, kPathAt + kNameSkip, kSizeAt}, &place) || place == 0) return std::nullopt;
    return Stream{place, m.cpu.rd32(kSizeAt)};
}

} // namespace vc2
