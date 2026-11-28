// The V-Synth's PATCH Init: see XtFwPatchInit.h (v473).
#include "XtFwPatchInit.h"
#include "XtFirmware.h"
#include "XtFwMachine.h"

using xtfile::fail;

namespace {
// [FW] rom.2's initialize: (store, name), a null name giving "INIT PATCH"
constexpr uint32_t kInit = 0x89482540, kNoName = 0;
constexpr uint32_t kStore = xtfw::kSamp;   // where the harness puts it (XtFwMachine.h's memory, as xtFwPatchStore does)
} // namespace

bool xtFwPatchInit(XtPatchFile& out, std::string* why) {
    out = {};
    xtfw::Machine m(xtFwRomPath());
    if (!m.ok) return fail(why, "no firmware image");
    if (!m.cpu.call(kInit, { kStore, kNoName }, xtfw::kStack)) {
        if (why) *why = "the firmware's PATCH Init failed: " + m.cpu.error();
        return false;
    }
    out.store.resize(static_cast<size_t>(kXtPatchBytes));
    m.cpu.read(kStore, out.store.data(), out.store.size());
    out.name = xtPatchStoreName(out.store);
    return true;
}
