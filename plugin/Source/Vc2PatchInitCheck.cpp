// xt_vc2patch's PATCH Init by Patch Category (v473; Vc2PatchInit.h), XT_VC2PATCH_MAIN only.  rom.4's window's own
// EXECUTE, run at each of the 12 Categories: part 0's entry in the part table holding a disk image patch, the
// Category its buttons chose at 0x89abb75c, and the button's callback (0x89493f40) on EXECUTE: part 0's store must be
// vc2PatchInit's of that Category byte for byte, its Patch Category the one chosen, its name "INIT PATCH", and the
// patch's change (0x89325360) asked for part 0.  CANCEL moves nothing.  The other window's EXECUTE (0x8943da70) gives
// Category 0's.  The default a .p02 is read over (0x89325300) is the routine given an empty name at Category 0, which
// rom.4 takes as none (0x89329312…0x8932931a), unlike rom.2: so it is Category 0's PATCH Init itself.  Each Category's
// name two lines, as rom.4 draws it.  The inits' stores are read besides the image's patches by the store check's
// reads (Vc2PatchStoreCheck.cpp).
#ifdef XT_VC2PATCH_MAIN
#include "Vc2PatchCheck.h"
#include "Vc2PatchFile.h"
#include "Vc2PatchInit.h"
#include "Vc2PatchStore.h"
#include "XtFiles.h"
#include "XtPatchInitWindow.h"
#include <set>

namespace {

// [FW] the window's button callback (its button word: 1 EXECUTE, 0 CANCEL) and the Category its buttons chose; what
// EXECUTE calls past the init (the patch's change, the screen's refresh, its move to the edit screen) and CANCEL's
// close.  The other window's callback, whose EXECUTE gives Category 0.
constexpr uint32_t kOnButton = 0x89493f40, kOtherOnButton = 0x8943da70, kChosen = 0x89abb75c;
constexpr XtInitCalls kCalls { 0x89325360, 0x8940b5b0, 0x8940c230, 0x8940c280 };
constexpr int kEditScreen = 7, kCategoryId = 0x12c;
// [FW] the default patch's routine (1: no lock asked) and the store it inits there
constexpr uint32_t kDefault = 0x89325300, kDefaultStore = 0x89970939;
constexpr uint16_t kCancel = 0, kExecute = 1;
constexpr uint32_t kButton = 0x8C070000;   // the button's word, in Vc2Fw's RAM
constexpr const char* kInitName = "INIT PATCH";

using xtfile::storeBytesApart;

// rom.4 on a machine of its own, part 0 holding `over`, the callback's other calls hooked to `asked`
struct Window {
    Vc2Fw fw;
    XtInitAsked asked;
    bool ok = false;
    Window(const std::string& rom, const std::vector<uint8_t>& over) {
        if (!fw.start(rom.c_str())) return;
        vc2PointAtStores(fw);
        fw.cpu.write(kVc2StoreB, over.data(), over.size());
        asked.hook(fw.cpu, kCalls);
        ok = true;
    }
    bool press(uint32_t callback, uint16_t button, int category) {
        fw.cpu.wr32(kChosen, static_cast<uint32_t>(category));
        fw.cpu.wr16(kButton, button);
        return fw.call(callback, { kButton });
    }
    std::vector<uint8_t> part0() {
        std::vector<uint8_t> s(kVc2StoreBytes);
        fw.cpu.read(kVc2StoreB, s.data(), s.size());
        return s;
    }
};

// One Category's EXECUTE, by `callback`, against the init of `category`: the bytes apart, -1 when a run fails
long executed(const std::string& rom, const std::vector<uint8_t>& over, uint32_t callback, int category,
              Vc2PatchTally& t) {
    Vc2PatchFile init;
    Window w(rom, over);
    if (!vc2PatchInit(category, init) || !w.ok || !w.press(callback, kExecute, category)) return -1;
    const long apart = storeBytesApart(w.part0(), init.store);
    ++t.compared;
    if (apart != 0 || !w.asked.executed(kEditScreen) || init.name != kInitName
        || xtStoreValue(vc2Store(), init.store, kCategoryId) != category)
        t.differs("Category " + std::to_string(category) + ": " + std::to_string(apart) + " bytes apart, named "
                  + init.name + ", the change asked " + std::to_string(w.asked.changes) + " times");
    return apart;
}

// [FW] the routine that draws a patch's Category: its value by ID (0x8940f1c0, answered with the Category tried), then
// its label from the table by it, drawn by 0x894f4370 (the label at r4)
constexpr uint32_t kDrawCategory = 0x894958c0, kValueOf = 0x8940f1c0, kDrawLabel = 0x894f4370;

// The label rom.4 draws for each Category, run, against vc2PatchCategoryNames'
void namesAsDrawn(const std::string& rom, Vc2PatchTally& t) {
    const std::vector<std::string> names = vc2PatchCategoryNames();
    if (names.size() != (size_t)kVc2PatchCategories)
        t.differs("the Categories' names: " + std::to_string(names.size()));
    for (int c = 0; c < (int)names.size(); ++c) {
        Vc2Fw fw;
        uint32_t drawn = 0;
        fw.start(rom.c_str());
        fw.cpu.hook(kValueOf, [c](Sh4& s) { s.ret(static_cast<uint32_t>(c)); });
        fw.cpu.hook(kDrawLabel, [&drawn](Sh4& s) {
            drawn = s.r[4];
            s.ret(0);
        });
        ++t.compared;
        if (!fw.call(kDrawCategory, {}) || drawn == 0) {
            t.differs("Category " + std::to_string(c) + ": rom.4 drew no label");
            continue;
        }
        const std::string label = vc2CategoryLabelAt(fw.cpu, drawn), said = names[(size_t)c];
        if (label != said) t.differs("Category " + std::to_string(c) + ": rom.4 draws " + label + ", not " + said);
        // two lines, neither empty, said on one: a space between them, none at either end ("Process Type 1"'s second
        // has one), no newline left
        const bool twoLines = said.find(' ') != std::string::npos && said.front() != ' ' && said.back() != ' '
                              && said.find('\n') == std::string::npos;
        if (!twoLines) t.differs("Category " + std::to_string(c) + ": \"" + said + "\" isn't two lines");
    }
}

// The default under a .p02 against Category 0's PATCH Init: the bytes apart, -1 on a fault
long defaultApart(const std::string& rom) {
    Vc2Fw a;
    Vc2PatchFile init;
    if (!a.start(rom.c_str()) || !a.call(kDefault, { 1 }) || !vc2PatchInit(0, init)) return -1;
    std::vector<uint8_t> under(kVc2StoreBytes);
    a.cpu.read(kDefaultStore, under.data(), under.size());
    return storeBytesApart(under, init.store);
}

} // namespace

std::vector<Vc2PatchFile> vc2PatchInitStores() {
    std::vector<Vc2PatchFile> out(kVc2PatchCategories);
    for (int c = 0; c < kVc2PatchCategories; ++c)
        if (!vc2PatchInit(c, out[static_cast<size_t>(c)])) return {};
    return out;
}

long vc2PatchInits(const std::string& rom, const std::vector<Vc2PatchOnDisk>& files) {
    Vc2PatchTally t { "inits" };
    Vc2PatchFile over;
    if (files.empty() || !vc2PatchRead(files.front().bytes.data(), files.front().bytes.size(), over)) {
        t.differs("no patch of the image's to hold before the init");
        return t.say("Categories");
    }
    std::set<std::vector<uint8_t>> distinct;
    for (int c = 0; c < kVc2PatchCategories; ++c) {
        if (executed(rom, over.store, kOnButton, c, t) < 0) t.differs("Category " + std::to_string(c) + ": no run");
        Vc2PatchFile init;
        if (vc2PatchInit(c, init)) distinct.insert(init.store);
    }
    if (executed(rom, over.store, kOtherOnButton, 0, t) != 0)
        t.differs("the other window's EXECUTE isn't Category 0's");
    Window cancel(rom, over.store);
    const bool cancelled = cancel.ok && cancel.press(kOnButton, kCancel, 3);
    if (!cancelled || cancel.part0() != over.store || !cancel.asked.cancelled())
        t.differs("CANCEL moved the patch, or didn't close");
    namesAsDrawn(rom, t);
    const long underFile = defaultApart(rom);
    if (underFile != 0)
        t.differs("the default under a .p02 is " + std::to_string(underFile) + " bytes from Category 0's init");
    return t.say("checked: the EXECUTEs of the 12 Categories (" + std::to_string(distinct.size())
                 + " stores distinct) and the other window's against vc2PatchInit, the 12 names against the labels "
                 + "rom.4 draws; CANCEL, the default under a .p02");
}
#endif
