// A patch saved as the firmware saves one: see XtFwPatchFile.h (v602).
#include "XtFwPatchFile.h"
#include "XtFirmware.h"
#include "XtFwMachine.h"
#include <memory>

using xtfile::fail;

namespace {
// [FW] rom.2's own: the setters of the two banks, the store's file layout, the .p00 writer (path, file layout) and what
// it calls of the OS (open, write, close, and the remove it calls when a write comes up short)
constexpr uint32_t kSet = 0x894818b0, kSetTyped = 0x89481e20, kToFile = 0x8947de90, kWriter = 0x8948fc30;
constexpr uint32_t kOpen = 0x89484850, kWrite = 0x89484930, kClose = 0x89486d80, kRemove = 0x89484a60;
// [FW] the descriptor banks: 12 bytes an ID from 0x897a167c, 8 from 0x897a56d8 for the IDs past them
constexpr uint32_t kDescs = 0x897a167c, kDescBytes = 12, kTypedDescs = 0x897a56d8, kTypedBytes = 8;
// Where the harness puts them (XtFwMachine.h's memory, as xtFwPatchStore uses it)
constexpr uint32_t kStore = xtfw::kSamp, kFile = xtfw::kSamp + 0x2000, kPath = xtfw::kSamp + 0x4000;
constexpr uint32_t kHandle = 3, kWritten = 1;   // the handle the open hook gives; the writer's answer when it wrote all
constexpr char kPathText[] = "A:\\SAVE.P00";

bool failed(std::string* why, const char* what, const Sh4& c) {
    if (why) *why = std::string(what) + ": " + c.error();
    return false;
}

// A machine on the image with `store` at kStore; none (and why) for a store not a patch's size, or no image
std::unique_ptr<xtfw::Machine> withStore(const std::vector<uint8_t>& store, std::string* why) {
    if (store.size() != static_cast<size_t>(kXtPatchBytes)) {
        fail(why, "not a patch's store");
        return nullptr;
    }
    auto m = std::make_unique<xtfw::Machine>(xtFwRomPath());
    if (!m->ok) {
        fail(why, "no firmware image");
        return nullptr;
    }
    m->cpu.write(kStore, store.data(), store.size());
    return m;
}

// One value by its bank's setter, on the store at kStore; false for an ID outside a patch or a call that fails
bool setOne(Sh4& c, const XtPatchSet& v, std::string* why) {
    const bool typed = v.id >= kXtDescIds;
    const XtTypedDesc* t = typed ? xtTypedDesc(v.id) : nullptr;
    const int section = xtPatchSection(typed && t ? v.base + t->slot : v.id, v.zone, v.cosm);
    if (section < 0 || (typed && !t)) return fail(why, "an ID outside a patch");
    const uint32_t at = kStore + static_cast<uint32_t>(section), value = static_cast<uint32_t>(v.value);
    const uint32_t desc = kDescs + kDescBytes * static_cast<uint32_t>(typed ? v.base : v.id);
    const uint32_t typedDesc = kTypedDescs + kTypedBytes * static_cast<uint32_t>(v.id - kXtDescIds);
    const bool ok = typed ? c.call(kSetTyped, { at, desc, typedDesc, value }, xtfw::kStack)
                          : c.call(kSet, { at, desc, value }, xtfw::kStack);
    return ok || failed(why, "the firmware's setter failed", c);
}
} // namespace

bool xtFwPatchSet(std::vector<uint8_t>& store, const std::vector<XtPatchSet>& values, std::string* why) {
    const auto m = withStore(store, why);
    if (!m) return false;
    for (const XtPatchSet& v : values)
        if (!setOne(m->cpu, v, why)) return false;
    m->cpu.read(kStore, store.data(), store.size());
    return true;
}

bool xtFwPatchFile(const std::vector<uint8_t>& store, std::vector<uint8_t>& file, std::string* why) {
    file.clear();
    const auto m = withStore(store, why);
    if (!m) return false;
    Sh4& c = m->cpu;
    if (!c.call(kToFile, { kFile, kStore }, xtfw::kStack)) return failed(why, "the firmware's file layout failed", c);
    std::vector<uint8_t> written;
    bool removed = false;
    c.hook(kOpen, [](Sh4& s) { s.ret(kHandle); });
    c.hook(kWrite, [&written](Sh4& s) {
        std::vector<uint8_t> bytes(s.r[6]);
        s.read(s.r[5], bytes.data(), bytes.size());
        written.insert(written.end(), bytes.begin(), bytes.end());
        s.ret(s.r[6]);
    });
    c.hook(kClose, [](Sh4& s) { s.ret(0); });
    c.hook(kRemove, [&removed](Sh4& s) {
        removed = true;
        s.ret(0);
    });
    c.write(kPath, kPathText, sizeof kPathText);
    if (!c.call(kWriter, { kPath, kFile }, xtfw::kStack)) return failed(why, "the firmware's writer failed", c);
    if (removed || c.r[0] != kWritten) return fail(why, "the firmware's writer gave the file up");
    file = std::move(written);
    return true;
}
