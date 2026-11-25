// A VC-2 patch saved by rom.4's own routines: see Vc2PatchSave.h (v505).  Every address is rom.4's.
#include "Vc2PatchSave.h"
#include "Sh4Interp.h"
#include "Vc2Params.h"
#include "Vc2PatchFile.h"
#include "Vc2PatchStore.h"
#include "XtFwMachine.h"
#include <memory>

namespace {

constexpr uint32_t kToFile = 0x89324f20;   // [FW] (file layout, store): the store's blocks into the file's layout
// [FW] the setters (rom.2's 0x894818b0, 0x89481e20 at one distance): (section, desc, value) and (section, slot desc,
// typed desc, value); the descriptor banks they're given
constexpr uint32_t kSet = 0x89328680, kSetTyped = 0x89328bf0;
constexpr uint32_t kDescs = 0x8950f0b0, kDescBytes = 12, kTypedDescs = 0x895131a8, kTypedBytes = 8;
constexpr uint32_t kWriter = 0x893411b0;   // [FW] (path, file layout) → 1 when it wrote the whole file
// [FW] the OS's calls the writer makes: open for writing (path, mode) a handle, write (handle, from, bytes) the bytes
// written, close (handle), and the removal it makes of a file a write came up short on (0x89341360)
constexpr uint32_t kOpen = 0x89336c90, kWrite = 0x89336d70, kClose = 0x893391c0, kRemove = 0x89336ea0;
// The machine's memory, as XtFwMachine lays it out: the store, the layout and the path in its samples' room
constexpr uint32_t kStore = xtfw::kSamp, kFile = kStore + 0x2000, kPath = kFile + 0x2000;
constexpr uint32_t kHandle = 3, kWritten = 1;
constexpr char kPathText[] = "A:\\Internal.vc2\\Patch\\001 SAVE.p02";

bool fail(std::string* why, const std::string& what) {
    if (why) *why = what;
    return false;
}

// The writer's file calls: one file, its bytes kept as written; whether the writer removed it
struct Written {
    std::vector<uint8_t> bytes;
    bool removed = false;
    void serve(Sh4& c) {
        c.hook(kOpen, [](Sh4& s) { s.ret(kHandle); });
        c.hook(kWrite, [this](Sh4& s) {
            const size_t at = bytes.size();
            bytes.resize(at + s.r[6]);
            s.read(s.r[5], bytes.data() + at, s.r[6]);
            s.ret(s.r[6]);
        });
        c.hook(kClose, [](Sh4& s) { s.ret(0); });
        c.hook(kRemove, [this](Sh4& s) {
            removed = true;
            s.ret(0);
        });
    }
};

// The machine of the rom.4 image v450's reader keeps, with `store` at kStore: none (and why) for a store not a patch's
// size, or no image kept or one that can't be read
std::unique_ptr<Sh4> rom4Holding(const std::vector<uint8_t>& store, std::string* why) {
    if (store.size() != (size_t)kVc2StoreBytes) {
        fail(why, "not a VC-2 patch's store");
        return nullptr;
    }
    auto cpu = std::make_unique<Sh4>();
    if (!vc2PatchFilesLoaded() || !cpu->loadRom(vc2PatchFilesRom().c_str())) {
        fail(why, "rom.4 can't be read");
        return nullptr;
    }
    cpu->mapRam(xtfw::kRam, xtfw::kRamSize);
    cpu->stepLimit = xtfw::kCallSteps;
    cpu->write(kStore, store.data(), store.size());
    return cpu;
}

// One value by rom.4's setter for its bank, on the store at kStore; false for an ID outside a patch or a failed call
bool rom4Sets(Sh4& c, const XtPatchSet& v, std::string* why) {
    const bool typed = v.id >= kVc2FirstTyped;
    const XtTypedDesc* t = typed ? vc2TypedDesc(v.id) : nullptr;
    const int base = typed ? (v.base ? v.base : vc2StoreSlotBase(v.id)) : v.id;
    const int section = xtStoreSection(vc2Store(), typed && t ? base + t->slot : v.id, v.zone, v.cosm);
    if (section < 0 || (typed && (!t || base < 0)) || (!typed && !vc2ParamDesc(v.id)))
        return fail(why, "an ID outside a VC-2 patch");
    const uint32_t at = kStore + (uint32_t)section, value = (uint32_t)v.value;
    const uint32_t desc = kDescs + kDescBytes * (uint32_t)base;
    const uint32_t typedDesc = kTypedDescs + kTypedBytes * (uint32_t)(v.id - kVc2FirstTyped);
    const bool ok = typed ? c.call(kSetTyped, { at, desc, typedDesc, value }, xtfw::kStack)
                          : c.call(kSet, { at, desc, value }, xtfw::kStack);
    return ok || fail(why, "rom.4's setter: " + c.error());
}

} // namespace

bool vc2PatchWrite(const std::vector<uint8_t>& store, std::vector<uint8_t>& file, std::string* why) {
    file.clear();
    const auto cpu = rom4Holding(store, why);
    if (!cpu) return false;
    if (!cpu->call(kToFile, { kFile, kStore }, xtfw::kStack)) return fail(why, "rom.4's file layout: " + cpu->error());
    Written written;
    written.serve(*cpu);
    cpu->write(kPath, kPathText, sizeof kPathText);
    uint32_t r0 = 0;
    if (!cpu->call(kWriter, { kPath, kFile }, xtfw::kStack, &r0)) return fail(why, "rom.4's writer: " + cpu->error());
    if (written.removed || r0 != kWritten) return fail(why, "rom.4's writer gave the file up");
    file = std::move(written.bytes);
    return true;
}

bool vc2PatchSet(std::vector<uint8_t>& store, const std::vector<XtPatchSet>& values, std::string* why) {
    const auto cpu = rom4Holding(store, why);
    if (!cpu) return false;
    for (const XtPatchSet& v : values)
        if (!rom4Sets(*cpu, v, why)) return false;
    cpu->read(kStore, store.data(), store.size());
    return true;
}
