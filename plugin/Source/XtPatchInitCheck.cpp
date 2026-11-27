// xt_files' PATCH Init (v473; XtFwPatchInit.h), XT_FILES_MAIN only.  The window's own EXECUTE, run: the 16 parts'
// patches made as the start-up makes them, part 0's then an image patch, and the button's callback (0x893c5a10) on
// EXECUTE: part 0's store must be xtFwPatchInit's byte for byte, the other parts' as they were, and the patch's change
// (0x8947e270) asked for part 0.  CANCEL moves nothing.  The default a file is read over (0x8947e230) is the same
// routine given an empty name: that run's store byte for byte.  It differs from the PATCH Init's in what 0x89482540
// writes only for a patch it names itself (its branch at 0x894828a0: the effects' switch and types, Matrix Control 5,
// each zone's tone and COSMs).  And the init's store against the firmware's reads, as the image's patches are checked
// (xtPatchCheckStore).  The callback's hooked calls are XtPatchInitWindow.h's.
#ifdef XT_FILES_MAIN
#include "XtFirmware.h"
#include "XtFwMachine.h"
#include "XtFwPatchInit.h"
#include "XtPatchInitWindow.h"
#include <cstdio>
#include <vector>

namespace {
// [FW] the start-up's making of the 16 parts' patches (r4 not 0: no lock asked), the part table its objects are found
// by (+0 the flag, +4 the patch), the window's button callback, and what its EXECUTE calls past the init: the patch's
// change (part, 1), the screen's refresh and its move to the edit screen; CANCEL's close.  And the init itself.
constexpr uint32_t kMakeParts = 0x89478510, kParts = 0x897a0fc8, kOnButton = 0x893c5a10, kInit = 0x89482540;
constexpr XtInitCalls kCalls { 0x8947e270, 0x893716b0, 0x89372450, 0x893724a0 };
constexpr int kPartCount = 16, kEditScreen = 6;
constexpr uint16_t kCancel = 0, kExecute = 1;
constexpr uint32_t kButton = xtfw::kSamp + 0x4000;   // the button's word, where the callback reads it
constexpr uint32_t kName = xtfw::kSamp + 0x5000;     // an empty name, as 0x8947e230 gives the init one
constexpr int kNameFirst = 0x103, kNameChars = 12;   // [FW] the patch name's IDs

// Where a part's patch is in the interpreter's memory, as the part table's object points at it
uint32_t partStoreAt(Sh4& c, int part) { return c.rd32(c.rd32(kParts + 4u * static_cast<uint32_t>(part)) + 4); }

std::vector<uint8_t> readPartStore(Sh4& c, int part) {
    std::vector<uint8_t> s(static_cast<size_t>(kXtPatchBytes));
    c.read(partStoreAt(c, part), s.data(), s.size());
    return s;
}

using xtfile::storeBytesApart;

// How many values two stores differ in, by ID in each zone and COSM, the name's passed over
long valuesApart(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    long n = 0;
    for (const XtPatchSectionDef& s : xtPatchSections())
        for (int id = s.first; id <= s.last; ++id)
            for (int z = 0; xtParamDesc(id) && z < (s.zoneStride ? 16 : 1); ++z)
                for (int k = 0; k < (xtPatchIsCosm(id) ? 2 : 1); ++k)
                    n += (id < kNameFirst || id >= kNameFirst + kNameChars)
                         && xtPatchValue(a, id, z, k) != xtPatchValue(b, id, z, k);
    return n;
}

// A machine with the parts' patches made, the callback's other calls hooked to `asked`, part 0's patch `over`
struct Window {
    xtfw::Machine m { xtFwRomPath() };
    XtInitAsked asked;
    bool ok = false;
    explicit Window(const std::vector<uint8_t>& over) {
        Sh4& c = m.cpu;
        if (!m.ok || !c.call(kMakeParts, { 1 }, xtfw::kStack)) return;
        asked.hook(c, kCalls);
        c.write(partStoreAt(c, 0), over.data(), over.size());
        ok = true;
    }
    bool press(uint16_t button) {
        m.cpu.wr16(kButton, button);
        return m.cpu.call(kOnButton, { kButton }, xtfw::kStack);
    }
};

// The first of the image's patches that reads, for part 0 to hold before the init
std::vector<uint8_t> anImagePatch(const XtDiskImage& disk) {
    for (const XtDiskImage::Entry& e : disk.list("Internal.prj/Patch")) {
        const std::vector<uint8_t> bytes = disk.read(e);
        XtPatchFile p;
        if (xtPatchRead(bytes.data(), bytes.size(), p)) return p.store;
    }
    return {};
}

// The init run with an empty name, and the default a file is read over (a file of no data): empty for either that fails
struct Defaults {
    std::vector<uint8_t> emptyNamed, underFile;
};
Defaults defaults() {
    static constexpr uint8_t kNoData[0x20] = { 0, 0x1e, 'S', 'V', 'D', '0', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                               'P', 'A', 'T', 'a', 'K', 'A', 'T', 'E', 0, 0, 0, 0x20, 0, 0, 0, 0 };
    Defaults d;
    xtfw::Machine m(xtFwRomPath());
    m.cpu.wr8(kName, 0);
    if (m.ok && m.cpu.call(kInit, { xtfw::kSamp, kName }, xtfw::kStack)) {
        d.emptyNamed.resize(static_cast<size_t>(kXtPatchBytes));
        m.cpu.read(xtfw::kSamp, d.emptyNamed.data(), d.emptyNamed.size());
    }
    XtPatchFile blank;
    if (xtPatchRead(kNoData, sizeof kNoData, blank)) d.underFile = blank.store;
    return d;
}
} // namespace

int xtPatchInitCheck(const XtDiskImage& disk, bool first) {
    XtPatchFile init;
    std::string why;
    if (!xtFwPatchInit(init, &why)) {
        std::printf("init: the PATCH Init failed: %s\n", why.c_str());
        return 1;
    }
    const std::vector<uint8_t> over = anImagePatch(disk);
    Window w(over);
    std::vector<std::vector<uint8_t>> others;
    for (int part = 1; w.ok && part < kPartCount; ++part) others.push_back(readPartStore(w.m.cpu, part));
    const bool ran = w.ok && !over.empty() && w.press(kExecute);
    const long executed = ran ? storeBytesApart(readPartStore(w.m.cpu, 0), init.store) : -1;
    long othersMoved = 0;
    for (int part = 1; ran && part < kPartCount; ++part)
        othersMoved += storeBytesApart(readPartStore(w.m.cpu, part), others[static_cast<size_t>(part - 1)]);
    const bool askedRight = w.asked.executed(kEditScreen);
    Window cancel(over);
    const bool cancelled = cancel.ok && cancel.press(kCancel);
    const long cancelMoved = cancelled ? storeBytesApart(readPartStore(cancel.m.cpu, 0), over) : -1;
    const bool cancelRight = cancel.asked.cancelled();
    const Defaults d = defaults();
    const bool haveDefaults = !d.emptyNamed.empty() && !d.underFile.empty();
    const long underFile = haveDefaults ? storeBytesApart(d.underFile, d.emptyNamed) : -1;
    const long ownName = haveDefaults ? valuesApart(d.emptyNamed, init.store) : -1;
    const XtPatchCheck reads = xtPatchCheckStore(init, first);
    const int wrong = (executed != 0) + (othersMoved != 0) + !askedRight + (cancelMoved != 0) + !cancelRight
                      + (underFile != 0) + (ownName <= 0) + (init.name != "INIT PATCH") + (reads.different != 0)
                      + (reads.outOfRange != 0);
    if (first && !askedRight) w.asked.say("init: EXECUTE");
    std::printf("init: EXECUTE (0x893c5a10) on part 0 over an image patch against xtFwPatchInit, %ld bytes different, "
                "the other 15 parts' %ld moved, the change asked %s; CANCEL %ld bytes moved, %s; named \"%s\"; "
                "the default under a file against the init named \"\", %ld bytes different, which is apart from "
                "this one in %ld values; %ld values against the firmware's reads, %ld out of range, %ld different; "
                "%d wrong\n",
                executed, othersMoved, askedRight ? "for part 0" : "wrongly", cancelMoved,
                cancelRight ? "closed" : "not closed", init.name.c_str(), underFile, ownName, reads.compared,
                reads.outOfRange, reads.different, wrong);
    return wrong;
}
#endif
