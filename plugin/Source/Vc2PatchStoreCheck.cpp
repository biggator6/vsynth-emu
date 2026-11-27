// xt_vc2patch <rom.4> [cases] [seed] (v450): rom.4's patch store and its .p02 reader against rom.4's own routines in the
// interpreter.  The store's sections: each of rom.4's ten pointing routines, and the one beside each on the store at +4,
// at every block, against the port's start of each of the section's IDs, a section's IDs from its first by rom.4's
// names to the next one's.  The reads: every ID a section holds in every block, its bits (0x89328580) and value
// (0x8932b050), and every typed ID through its block's slots (0x8932b460), the blocks by rom.4's type tables, on the
// disk image's 128 patches as vc2PatchRead stores them and `cases` stores of random bits (40, seed 2024).  The files:
// Vc2PatchFileCheck.cpp; rom.4's patch save (v505): Vc2PatchSaveCheck.cpp.  A line a part, its first differences under
// it.  XT_VC2PATCH_MAIN only.
#ifdef XT_VC2PATCH_MAIN
#include "Vc2PatchCheck.h"
#include "Vc2Params.h"
#include "Vc2PatchFile.h"
#include "Vc2PatchStore.h"
#include "XtFiles.h"
#include <algorithm>

namespace {

constexpr uint32_t kReadBits = 0x89328580;      // [FW] (a section's place, descriptor) → its bits
constexpr uint32_t kReadValue = 0x8932b050;     // [FW] (a section's place, id) → its value
constexpr uint32_t kReadTyped = 0x8932b460;     // [FW] (a section's place, slot base, id) → a typed ID's
constexpr uint32_t kDescriptors = 0x8950f0b0;   // [FW] the first bank, 12 bytes an ID
constexpr int kNoId = -1, kSlot3 = 3;

// A section as the check finds it: rom.4's routine that aims a read object at it (obj, part, block, cosm) and the one
// beside it on the store at +4, the name of its first ID, and the blocks and COSMs to try; its IDs run from its first
// to the next one's
struct Pointer {
    uint32_t routine, beside;
    const char* firstName;
    int blocks, cosms;
};
constexpr Pointer kPointers[] = {
    { 0x893207b0, 0x893207e0, "Patch Name 1", 1, 1 },    { 0x893208d0, 0x89320900, "Activity", 1, 1 },
    { 0x89320950, 0x89320990, "Chorus Type", 1, 1 },     { 0x893209d0, 0x89320a10, "Reverb Type", 1, 1 },
    { 0x89320a50, 0x89320a90, "Structure Type", 8, 1 },  { 0x89320ad0, 0x89320b20, "Env 1 Attack Time", 16, 1 },
    { 0x89320b70, 0x89320bd0, "LFO 1 Waveform", 16, 1 }, { 0x89320c10, 0x89320c90, "COSM Switch", 16, 2 },
    { 0x89320d10, 0x89320d50, "Beam Type", 1, 1 },       { 0x89320da0, 0x89320e00, "Control Switch", 16, 1 },
};
constexpr const char* kPastLast = "Sampling Template Name 1";   // the first ID after the store's
constexpr const char* kDiskImage = "V-SynthXTDiskImage.bin.0.bin";

// [FW] the typed IDs' blocks by their type tables (s16 pairs, a type's first and last typed ID; 0x8935f536, 0x8935f604,
// 0x8935f6a2 and 0x89399cfa load them), and the name of the first-bank ID that is each block's slot 3
struct TypedTable {
    uint32_t table;
    const char* slot3Name;
};
constexpr TypedTable kTypedTables[] = { { 0x895164ec, "MFX Parameter 1" }, { 0x895165b8, "Chorus Parameter 1" },
                                        { 0x895165e4, "Reverb Parameter 1" }, { 0x8953fb40, "COSM Parameter 1" } };

struct Found {
    const Pointer* p;
    int first, last;
};
struct FoundTyped {
    int first, slotBase;
    const Found* section;
};

std::string hex(long v) {
    char s[16];
    std::snprintf(s, sizeof s, "%#lx", v);
    return s;
}

std::string where(int id, int block, int cosm) {
    return hex(id) + " block " + std::to_string(block) + " cosm " + std::to_string(cosm);
}

int idNamed(const char* name, int from) {
    for (int id = from; id < kVc2Ids; ++id)
        if (vc2ParamName(id) == name) return id;
    return kNoId;
}

// The sections by rom.4's names, in order; empty when a name isn't there
std::vector<Found> findSections() {
    std::vector<Found> out;
    int from = 0;
    for (const Pointer& p : kPointers) {
        const int first = idNamed(p.firstName, from);
        if (first == kNoId) return {};
        if (!out.empty()) out.back().last = first - 1;
        out.push_back({ &p, first, first });
        from = first + 1;
    }
    const int past = idNamed(kPastLast, from);
    if (past == kNoId) return {};
    out.back().last = past - 1;
    return out;
}

std::vector<FoundTyped> findTyped(Vc2Fw& fw, const std::vector<Found>& found) {
    std::vector<FoundTyped> out;
    for (const TypedTable& t : kTypedTables) {
        const int slotBase = idNamed(t.slot3Name, 0) - kSlot3;
        const auto holds = [slotBase](const Found& f) { return f.first <= slotBase && f.last >= slotBase; };
        const auto s = std::ranges::find_if(found, holds);
        if (s == found.end()) return {};
        out.push_back({ (int16_t)fw.cpu.rd16(t.table), slotBase, &*s });
    }
    return out;
}

// Part 0's entry in the part table: a store at +8 (A) and +4 (B)
void pointAtStores(Vc2Fw& fw) {
    fw.cpu.wr8(kVc2Entry, 1);
    fw.cpu.wr32(kVc2Entry + 4, kVc2StoreB);
    fw.cpu.wr32(kVc2Entry + 8, kVc2StoreA);
    fw.cpu.wr32(kVc2Parts, kVc2Entry);
}

// Where rom.4's routine aims a read object: its +4 less the store; -1 on a fault
long aimed(Vc2Fw& fw, uint32_t routine, uint32_t store, int block, int cosm) {
    fw.cpu.wr32(kVc2Object + 4, 0);
    if (!fw.call(routine, { kVc2Object, 0, (uint32_t)block, (uint32_t)cosm })) return -1;
    return (long)fw.cpu.rd32(kVc2Object + 4) - (long)store;
}

// Every routine and the one beside it at every block, against the port's start of each of the section's IDs; and no
// ID outside them placed
long sections(Vc2Fw& fw, const std::vector<Found>& found) {
    Vc2PatchTally t { "sections" };
    for (const Found& f : found)
        for (int c = 0; c < f.p->cosms; ++c)
            for (int b = 0; b < f.p->blocks; ++b) {
                const long at = aimed(fw, f.p->routine, kVc2StoreA, b, c);
                if (aimed(fw, f.p->beside, kVc2StoreB, b, c) != at) t.differs(hex(f.p->beside) + " aims elsewhere");
                for (int id = f.first; id <= f.last; ++id, ++t.compared)
                    if (const int mine = xtStoreSection(vc2Store(), id, b, c); mine != at)
                        t.differs(where(id, b, c) + ": at " + hex(mine) + ", rom.4's " + hex(f.p->routine) + " at "
                                  + hex(at));
            }
    for (int id = 0; id < kVc2FirstTyped; ++id)
        if (id < found.front().first || id > found.back().last) {
            ++t.compared;
            if (xtStoreSection(vc2Store(), id) != -1) t.differs(hex(id) + " is placed, and no section holds it");
        }
    return t.say("IDs placed, at every block, against rom.4's pointing routines");
}

// Every ID a section holds in every block, its bits and value, on the store at A
void reads(Vc2Fw& fw, const std::vector<Found>& found, const std::vector<uint8_t>& store, const std::string& which,
           Vc2PatchTally& t) {
    for (const Found& f : found)
        for (int c = 0; c < f.p->cosms; ++c)
            for (int b = 0; b < f.p->blocks; ++b) {
                const uint32_t at = kVc2StoreA + (uint32_t)aimed(fw, f.p->routine, kVc2StoreA, b, c);
                for (int id = f.first; id <= f.last; ++id, ++t.compared) {
                    uint32_t bits = 0, value = 0;
                    const bool ok = fw.call(kReadBits, { at, kDescriptors + 12u * (uint32_t)id }, &bits)
                                    && fw.call(kReadValue, { at, (uint32_t)id }, &value);
                    const int myBits = xtStoreRaw(vc2Store(), store, id, b, c);
                    const int myValue = xtStoreValue(vc2Store(), store, id, b, c);
                    if (!ok || (int)bits != myBits || (int)value != myValue)
                        t.differs(which + ": " + where(id, b, c) + ": " + std::to_string(myBits) + " → "
                                  + std::to_string(myValue) + ", rom.4's " + std::to_string((int)bits) + " → "
                                  + std::to_string((int)value));
                }
            }
}

// Every typed ID in every block its slots are in, against rom.4's typed reader on its block's slot base
void typedReads(Vc2Fw& fw, const std::vector<FoundTyped>& blocks, const std::vector<uint8_t>& store,
                const std::string& which, Vc2PatchTally& t) {
    for (int id = kVc2FirstTyped; id < kVc2Ids; ++id) {
        const auto after = [id](const FoundTyped& f) { return id >= f.first; };
        const auto k = std::ranges::find_if(blocks.rbegin(), blocks.rend(), after);
        if (k == blocks.rend()) continue;
        const Pointer& p = *k->section->p;
        for (int c = 0; c < p.cosms; ++c)
            for (int b = 0; b < p.blocks; ++b, ++t.compared) {
                const uint32_t at = kVc2StoreA + (uint32_t)aimed(fw, p.routine, kVc2StoreA, b, c);
                uint32_t value = 0;
                const bool ok = fw.call(kReadTyped, { at, (uint32_t)k->slotBase, (uint32_t)id }, &value);
                const int mine = vc2StoreTyped(store, id, b, c);
                if (!ok || (int)value != mine)
                    t.differs(which + ": " + where(id, b, c) + ": " + std::to_string(mine) + ", rom.4's "
                              + std::to_string((int)value));
            }
    }
}

// The disk image beside rom.4: Internal.vc2's patches
std::vector<Vc2PatchOnDisk> patchesOnDisk(const std::string& rom) {
    const auto slash = rom.find_last_of('/');
    XtDiskImage disk;
    if (!disk.open((slash == std::string::npos ? std::string() : rom.substr(0, slash + 1)) + kDiskImage)) return {};
    std::vector<Vc2PatchOnDisk> out;
    for (const XtDiskImage::Entry& e : disk.list("Internal.vc2/Patch"))
        if (!e.isDir) out.push_back({ e.name, disk.read(e) });
    return out;
}

void saySections(const std::vector<Found>& found, const std::vector<FoundTyped>& typed) {
    std::printf("sections from");
    for (const Found& f : found) std::printf(" %#x", f.first);
    std::printf(" to %#x; typed blocks from", found.back().last);
    for (const FoundTyped& k : typed) std::printf(" %#x (slots from %#x)", k.first, k.slotBase);
    std::printf("\n");
}

} // namespace

int main(int argc, char** argv) {
    Vc2Fw fw;
    const auto run = vc2Start(argc, argv, fw, "xt_vc2patch", vc2LoadAll<vc2LoadParams, vc2LoadPatchFiles>);
    if (!run) return 2;
    const std::vector<Vc2PatchOnDisk> files = patchesOnDisk(argv[1]);
    const std::vector<Found> found = findSections();
    const std::vector<FoundTyped> typed = findTyped(fw, found);
    if (files.empty() || typed.empty()) {
        std::printf("xt_vc2patch: no disk image beside rom.4, or a section's name isn't rom.4's\n");
        return 1;
    }
    saySections(found, typed);
    pointAtStores(fw);
    long different = sections(fw, found);
    Vc2PatchTally values { "reads" }, slots { "typed reads" };
    std::mt19937 rng((unsigned)run->seed);
    for (int s = 0; s < (int)files.size() + run->cases; ++s) {
        const bool onDisk = s < (int)files.size();
        std::vector<uint8_t> store(kVc2StoreBytes);
        Vc2PatchFile read;
        if (onDisk && vc2PatchRead(files[(size_t)s].bytes.data(), files[(size_t)s].bytes.size(), read))
            store = read.store;
        else
            for (uint8_t& b : store) b = (uint8_t)rng();
        const std::string which = onDisk ? files[(size_t)s].name : "random store " + std::to_string(s);
        fw.cpu.write(kVc2StoreA, store.data(), store.size());
        reads(fw, found, store, which, values);
        typedReads(fw, typed, store, which, slots);
    }
    const std::string over = std::to_string(files.size()) + " patches and " + std::to_string(run->cases) + " random";
    different += values.say("values in every block, bits and value, over " + over);
    different += slots.say("typed values in every zone, over " + over);
    different += vc2PatchFiles(fw, files);
    different += vc2PatchSaves(files, run->cases, run->seed);
    different += vc2PatchSetters(files, run->seed);
    std::printf("xt_vc2patch: %ld different\n", different);
    return different ? 1 : 0;
}
#endif
