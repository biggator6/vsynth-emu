// The VC-2's PATCH Init by Patch Category: see Vc2PatchInit.h (v473).  Every address is rom.4's.
#include "Vc2PatchInit.h"
#include "Sh4Interp.h"
#include "Vc2PatchStore.h"
#include "XtFwMachine.h"
#include <algorithm>

namespace {

constexpr uint32_t kInit = 0x893291c0, kNoName = 0;   // [FW] (store, name, category), a null name giving "INIT PATCH"
constexpr uint32_t kCategoryNames = 0x8994a638;        // [FW] a pointer a Category: its label, a newline between lines
constexpr uint32_t kStore = xtfw::kSamp;               // the store, in XtFwMachine's samples' room
constexpr uint32_t kLabelMost = 32;                    // a label's characters at the most, its NUL not counted

bool fail(std::string* why, const std::string& what) {
    if (why) *why = what;
    return false;
}

// A machine on the rom.4 image the patch files' reads run on: false (and why) before one is kept
bool onRom4(Sh4& cpu, std::string* why) {
    if (!vc2PatchFilesLoaded()) return fail(why, "rom.4 isn't loaded");
    if (!cpu.loadRom(vc2PatchFilesRom().c_str())) return fail(why, "rom.4 can't be read again");
    xtfw::forCalls(cpu);
    return true;
}

// A NUL-ended string at `at`, at most kLabelMost characters
std::string textAt(Sh4& cpu, uint32_t at) {
    std::string s;
    for (uint32_t k = 0; k < kLabelMost; ++k) {
        const char ch = static_cast<char>(cpu.rd8(at + k));
        if (ch == '\0') break;
        s += ch;
    }
    return s;
}

} // namespace

bool vc2PatchInit(int category, Vc2PatchFile& out, std::string* why) {
    out = {};
    if (category < 0 || category >= kVc2PatchCategories) return fail(why, "no such Patch Category");
    Sh4 cpu;
    if (!onRom4(cpu, why)) return false;
    if (!cpu.call(kInit, { kStore, kNoName, static_cast<uint32_t>(category) }, xtfw::kStack))
        return fail(why, "rom.4's PATCH Init failed: " + cpu.error());
    vc2PatchTake(cpu, kStore, out);
    return true;
}

std::string vc2CategoryLabelAt(Sh4& cpu, uint32_t at) {
    std::string label = textAt(cpu, at);
    std::replace(label.begin(), label.end(), '\n', ' ');   // [FW] its two lines, a newline between
    return label;
}

std::vector<std::string> vc2PatchCategoryNames() {
    Sh4 cpu;
    std::vector<std::string> out;
    if (!onRom4(cpu, nullptr)) return out;
    for (uint32_t c = 0; c < static_cast<uint32_t>(kVc2PatchCategories); ++c)
        out.push_back(vc2CategoryLabelAt(cpu, cpu.rd32(kCategoryNames + 4 * c)));
    return out;
}
