// vc2card <rom.4>: where Bank Select and Program Change reach a patch on a card in VC-2 mode, by rom.4's own boot
// (research/VC2_MODE_PLAN.md, step 5; HARDWARE_FIRMWARE.md, "A card's patches in VC-2 mode").  Vc2WaveBoot's machine
// (v503) answers the kernel's file and folder calls from a made-up card (vc2cardfiles.h): "C:\VC-2 Project.ini" and a
// project folder whose Patch\ holds the disk image's factory patches under numbers of our choosing.  Then the bank's
// static initializer (0x8931a940) and the boot's own stages in its order (0x8933eea0): the project's path (0x89342cb0),
// the bank's set-up (0x8931a000 … 0x89319a20) and the Patch folder's load (0x89341e90); the waves' stages, which none
// of these read, are left out.  What the bank holds (0x899613e8: +0 its first patch, +4 the count, 0x13a4 bytes a
// patch) is read back by name, and rom.4's own parse of a part's Bank Select and Program Change (0x89318a20) is run on
// every bank and number.  A kernel entry the boot's table doesn't answer is given 0, and named.
// Build: clang++ -std=c++20 -O2 -Iplugin/Source -Iresearch/tools research/tools/vc2card.cpp
//        research/tools/vc2cardkernel.cpp plugin/Source/Vc2WaveBoot.cpp plugin/Source/Vc2WaveBootKernel.cpp
//        plugin/Source/Vc2WaveStore.cpp plugin/Source/Vc2WaveSets.cpp plugin/Source/Vc2WavePool.cpp
//        plugin/Source/Vc2PartSettingsWords.cpp plugin/Source/Vc2PatchStore.cpp plugin/Source/XtPatchStore.cpp
//        plugin/Source/Vc2Params.cpp plugin/Source/XtFxValueText.cpp plugin/Source/XtFirmware.cpp
//        plugin/Source/XtFwMachine.cpp plugin/Source/XtMultiBandPlayer.cpp plugin/Source/XtSoloVoice.cpp
//        plugin/Source/XtDiskImage.cpp plugin/Source/Sh4Interp.cpp plugin/Source/Sh4Setup.cpp -o build/tools/vc2/vc2card
#include "Vc2Params.h"
#include "Vc2PatchFile.h"
#include "Vc2PatchStore.h"
#include "Vc2WaveBoot.h"
#include "XtFiles.h"
#include "vc2cardfiles.h"
#include <cstdio>

namespace {

using vc2card::CardFiles;
using vc2card::heldText;
using vc2card::Lines;

constexpr uint32_t kProject = 0x8997ab20;        // [FW] the project object; its path by 0x893436b0
constexpr uint32_t kProjectPath = 0x89342cb0, kPathOf = 0x893436b0;   // [FW]
constexpr uint32_t kBank = 0x899613e8;           // [FW] +0 the first patch, +4 the count (a halfword)
constexpr uint32_t kPatchBytes = 0x13a4;         // [FW] a patch's store in the bank
constexpr uint32_t kParse = 0x89318a20;          // [FW] (record: +0 PC, +1 MSB, +2 LSB; out: +0, +1, +2 the number)
// [FW] the boot's stages after the project's path, each on a scratch object of the boot's frame, and the folder's load
constexpr uint32_t kBankStages[] = {0x8931a000, 0x89319ec0, 0x8931a1a0, 0x8931a0f0, 0x89319e20, 0x89319df0,
                                    0x89319ac0, 0x89319a20};
constexpr uint32_t kPatchFolder = 0x89341e90;
// [FW] the bank's static initializer (a row of the init table): 512 patches at 0x89addbf8, each the init patch
constexpr uint32_t kMakeBank = 0x8931a940;
// In the RAM the boot maps (0x8C000000, 1 MB), well below its stack (0x8C0E0000)
constexpr uint32_t kScratch = 0x8C0A0000, kRecord = 0x8C0A1000, kOut = 0x8C0A1010;
constexpr int kNameFirst = 0x120, kNameChars = 12;   // [FW] Patch Name 1-12
constexpr const char* kDiskImage = "V-SynthXTDiskImage.bin.0.bin";

std::string storeName(const std::vector<uint8_t>& store) {
    std::string name;
    for (int k = 0; k < kNameChars; ++k) name += (char)xtStoreRaw(vc2Store(), store, kNameFirst + k);
    while (!name.empty() && name.back() == ' ') name.pop_back();
    return name;
}

// A file's patch name as v450's reader reads it (rom.4's reader on its own file calls), to know it in the bank
std::string storeNameOfFile(const std::vector<uint8_t>& file) {
    Vc2PatchFile read;
    return vc2PatchRead(file.data(), file.size(), read) ? read.name : "(not read)";
}

struct Card {
    const char* what;
    std::string ini;        // empty: no "C:\VC-2 Project.ini"
    std::string project;    // the folder the patches go in
};

// The disk image's factory patches, Internal.vc2/Patch/001…064
std::vector<std::vector<uint8_t>> factory(const std::string& rom4) {
    const size_t slash = rom4.rfind('/');
    XtDiskImage disk;
    std::vector<std::vector<uint8_t>> out;
    if (!disk.open((slash == std::string::npos ? std::string() : rom4.substr(0, slash + 1)) + kDiskImage)) return out;
    for (const XtDiskImage::Entry& e : disk.list("Internal.vc2/Patch"))
        if (!e.isDir && out.size() < 64) out.push_back(disk.read(e));
    return out;
}

// The numbers the card's files are given, each a factory patch in turn; then two names rom.4's parse refuses
constexpr int kNumbers[] = {1, 2, 64, 65, 66, 128, 129, 300, 448, 449, 511, 512};

CardFiles makeCard(const Card& card, const std::vector<std::vector<uint8_t>>& f) {
    CardFiles files;
    if (!card.ini.empty()) files.put("C:\\VC-2 Project.ini", std::vector<uint8_t>(card.ini.begin(), card.ini.end()));
    files.putFolder(card.project);
    files.putFolder(card.project + "Patch\\");
    files.putFolder(card.project + "Wave\\");
    for (size_t k = 0; k < std::size(kNumbers) && k < f.size(); ++k) {
        char name[32];
        std::snprintf(name, sizeof name, "%03d CARD%02zu.p02", kNumbers[k], k);
        files.put(card.project + "Patch\\" + name, f[k]);
        std::printf("   %s holds the data of %s\n", name, storeNameOfFile(f[k]).c_str());
    }
    files.put(card.project + "Patch\\513 PAST.p02", f.front());
    files.put(card.project + "Patch\\ABC NAME.p02", f.front());
    return files;
}

bool runStages(Sh4& c) {
    uint32_t r0 = 0;
    if (!c.call(kMakeBank, {}, Vc2WaveBoot::kStack, &r0)) return false;
    if (!c.call(kProjectPath, {kProject}, Vc2WaveBoot::kStack, &r0)) return false;
    for (const uint32_t at : kBankStages)
        if (!c.call(at, {kScratch}, Vc2WaveBoot::kStack, &r0)) return false;
    return c.call(kPatchFolder, {}, Vc2WaveBoot::kStack, &r0);
}

void printBank(Sh4& c) {
    const uint32_t first = c.rd32(kBank), count = c.rd16(kBank + 4);
    std::printf("   the bank: %u patches from %08x; each one not the init patch, by name:\n", count, first);
    for (uint32_t n = 1; first && n <= count; ++n) {
        std::vector<uint8_t> store(kPatchBytes);
        c.read(first + kPatchBytes * (n - 1), store.data(), store.size());
        const std::string name = storeName(store);
        if (!name.empty() && name != "INIT PATCH") std::printf("     %3u  %s\n", n, name.c_str());
    }
}

// rom.4's parse of a part's Bank Select MSB, LSB and Program Change on the bank as the boot left it
void printParse(Sh4& c) {
    std::printf("   Bank Select and Program Change, by 0x89318a20 (MSB LSB: PC -> group number; 0 0 refused):\n");
    for (const int msb : {0, 86, 87, 88, 127}) {
        for (const int lsb : {0, 1, 2, 3, 4, 5}) {
            std::printf("     %3d %d:", msb, lsb);
            for (const int pc : {0, 1, 63, 64, 127}) {
                c.wr8(kRecord, (uint8_t)pc);
                c.wr8(kRecord + 1, (uint8_t)msb);
                c.wr8(kRecord + 2, (uint8_t)lsb);
                c.call(kParse, {kRecord, kOut}, Vc2WaveBoot::kStack);
                std::printf("  %3d -> %d %3d", pc, c.rd8(kOut + 1), c.rd16(kOut + 2));
            }
            std::printf("\n");
            if (msb != 87) break;
        }
    }
}

void runCard(const std::string& rom4, const Card& card, const std::vector<std::vector<uint8_t>>& f) {
    std::printf("%s\n", card.what);
    const CardFiles files = makeCard(card, f);
    Vc2WaveBoot boot(rom4, files);
    std::string why;
    std::vector<std::string> reached;
    Lines lines{card.ini};
    if (!boot.start(&why) || !boot.initialize(&why)) return (void)std::printf("   the boot: %s\n", why.c_str());
    Sh4& c = boot.cpu();
    vc2card::answerTheRest(c, reached, lines);
    vc2card::answerHandleName(c, files, reached);
    vc2card::answerSeekPlace(c);
    const bool ran = runStages(c);
    std::printf("   what it asked for:");
    for (const std::string& a : files.asked) std::printf(" [%s]", a.c_str());
    std::printf("\n   kernel entries given 0:");
    for (const std::string& r : reached) std::printf(" %s", r.c_str());
    std::printf("\n");
    if (!ran) return (void)std::printf("   a stage stopped: %s at %08x\n", c.error().c_str(), c.pc);
    uint32_t path = 0;
    c.call(kPathOf, {kProject}, Vc2WaveBoot::kStack, &path);
    std::printf("   the project rom.4 took: %s\n", heldText(c, path).c_str());
    printBank(c);
    if (c.rd16(kBank + 4)) printParse(c);
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: vc2card <rom.4>\n");
        return 2;
    }
    const std::vector<std::vector<uint8_t>> f = factory(argv[1]);
    if (!vc2LoadParams(argv[1]) || !vc2LoadPatchFiles(argv[1]) || f.empty()) {
        std::fprintf(stderr, "vc2card: rom.4's tables can't be read, or no disk image with Internal.vc2/Patch\n");
        return 1;
    }
    const std::string head = "[current project]\r\n", card = "drive=\"PC Card\"\r\n";
    const std::string path = "path=\"RUNS.vc2\\\"\r\n";
    const Card cards[] = {
        {"1. drive=\"PC Card\", path=\"RUNS.vc2\\\"", head + card + path, "C:\\RUNS.vc2\\"},
        {"2. drive=\"PC Card\", path=\"RUNS.vc2\" (no \\)", head + card + "path=\"RUNS.vc2\"\r\n", "C:\\RUNS.vc2\\"},
        {"3. drive=\"PC Card\", the folder missing", head + card + "path=\"GONE.vc2\\\"\r\n", "C:\\RUNS.vc2\\"},
        {"4. drive=\"Internal\", path=\"RUNS.vc2\\\"", head + "drive=\"Internal\"\r\n" + path, "A:\\RUNS.vc2\\"},
        {"5. no ini on the card: the internal project", "", "A:\\Internal.vc2\\"}};
    for (const Card& c : cards) runCard(argv[1], c, f);
    return 0;
}
