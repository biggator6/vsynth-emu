// xt_files' patches (XT_FILES_MAIN; its main is XtFiles.cpp's, v187): the disk image's preset patches, every value in the
// store against the firmware's reads of it (0x894817b0, the bits; 0x89484310, the value), a type's parameters against its
// read of them (0x89481d70), the blocks with no zone through the firmware's own routines that point a read object at them
// and its getters (0x8947d310, 0x8947d330), and an older, shorter patch.
#ifdef XT_FILES_MAIN
#include "XtFiles.h"
#include "XtFirmware.h"
#include "XtFwMachine.h"
#include <algorithm>
#include <cstdio>
#include <vector>

namespace {
using xtfile::be32;
using xtfile::fnv;

// One patch's every value, each against the firmware's reads when `fw` holds it
void compareValues(XtPatchCheck& c, std::vector<long>& outBy, const XtPatchFile& p, XtFwPatchReader* fw, bool first) {
    for (const XtPatchSectionDef& s : xtPatchSections())
        for (int id = s.first; id <= s.last; ++id) {
            const XtParamDesc* d = xtParamDesc(id);
            if (!d) continue;
            for (int z = 0; z < (s.zoneStride ? 16 : 1); ++z)
                for (int k = 0; k < (xtPatchIsCosm(id) ? 2 : 1); ++k) {
                    const int raw = xtPatchRaw(p.store, id, z, k), value = xtPatchValue(p.store, id, z, k);
                    ++c.values;
                    // 0x176 lies over the MFX Type, and isn't a patch's
                    if (id != 0x176 && d->min != d->max && (raw < d->min || raw > d->max)) {
                        ++c.outOfRange;
                        ++outBy[(size_t)id];
                    }
                    if (!fw) continue;
                    int fwRaw = 0, fwValue = 0;
                    ++c.compared;
                    const bool read = fw->read(xtPatchSection(id, z, k), id, fwRaw, fwValue);
                    if (read && fwRaw == raw && fwValue == value) continue;
                    ++c.different;
                    if (first && c.different < 20)
                        std::printf("patch %s id %03x zone %d cosm %d: raw %d, firmware %d; value %d, firmware %d\n",
                                    p.name.c_str(), id, z, k, raw, fwRaw, value, fwValue);
                }
        }
}

// A type's parameters, the slots' meaning under the COSMs' and the effects' types, against the firmware's read of them:
// every type's IDs on one patch (`fw` holding it), in range or not.  Into n, and wrong those that differ.
void compareTyped(long& n, long& wrong, const XtPatchFile& p, XtFwPatchReader& fw, bool first) {
    const auto one = [&](int base, int id, int section, int z, int c) {
        int fwValue = 0;
        ++n;
        const int here = xtPatchTyped(p.store, base, id, z, c);
        if (fw.readTyped(section, base, id, fwValue) && fwValue == here) return;
        ++wrong;
        if (first && wrong < 20)
            std::printf("patch %s base %03x id %03x zone %d cosm %d: firmware %d, here %d\n",
                        p.name.c_str(), base, id, z, c, fwValue, here);
    };
    // block 3, the COSMs, in zones 0, 5, 10 and 15; then the MFX, Chorus and Reverb
    static constexpr int kBases[4] = { kXtMfxSlots, kXtChorusSlots, kXtReverbSlots, kXtCosmSlots };
    for (const int b : { 3, 0, 1, 2 })
        for (int z = 0; z < (b == 3 ? 16 : 1); z += 5)
            for (int c = 0; c < (b == 3 ? 2 : 1); ++c)
                for (int t = 0; t < xtEffectTypes(b); ++t) {
                    int lo = 0, hi = -1;
                    if (!xtEffectTypeIds(b, t, lo, hi)) {
                        ++wrong;
                        continue;
                    }
                    for (int id = lo; id <= hi; ++id) one(kBases[b], id, xtPatchSection(kBases[b], z, c), z, c);
                }
}

// The disk image's patches whose values the firmware reads: one in eight, as they're the slow part
bool everyEighth(int nth) { return nth % 8 == 0; }

// The blocks with no zone that the firmware points a read object at, each by a pair of its routines (object, part, line),
// for a part's two stores, its object's +8 and +4: the IDs a block holds, and how many lines of it there are (v187)
struct Pointed {
    int firstId, lastId;
    uint32_t storeTwo, storeOne;
    int lines;
};
constexpr Pointed kPointed[] = {
    { 0x42b, 0x438, 0x894799a0, 0x894799e0, 1 },    // +0x1286: Beam Type, Pad Mode, Pad Hold, the arpeggio
    { 0x439, 0x448, 0x89479a40, 0x89479a80, 1 },    // +0x14f2: the zones' Knob Sets
    { 0x449, 0x48c, 0x89479ad0, 0x89479b20, 16 },   // +0x128c + 34 x the line: the arpeggio pattern
    { 0x48d, 0x4ef, 0x89479b70, 0x89479bb0, 1 },    // +0x14ac: the Multi Step Modulator
};

// The firmware's own way to a value, on a machine of its own: the part table's (0x897a0fc8) first object given its flag
// (+0, 1) and both its stores (+4, +8) as the patch, a read object pointed at a block by the routine the firmware calls
// for it, and the getters through that
class FwGetter {
public:
    FwGetter() : m(xtFwRomPath()) {
        const uint32_t part = m.cpu.rd32(kParts);
        m.cpu.wr8(part, 1);
        m.cpu.wr32(part + 4, xtfw::kSamp);
        m.cpu.wr32(part + 8, xtfw::kSamp);
    }
    void load(const std::vector<uint8_t>& store) { m.cpu.write(xtfw::kSamp, store.data(), store.size()); }
    // Where `routine` points the read object for `line` of part 0's store: 0 if the call fails
    uint32_t point(uint32_t routine, int line) {
        if (!m.cpu.call(routine, { kObject, 0, (uint32_t)line }, xtfw::kStack)) return 0;
        return m.cpu.rd32(kObject + 4);
    }
    // 0x8947d310 on a block, and 0x8947d330, which must agree
    bool get(uint32_t block, int id, int& value) {
        uint32_t a = 0;
        uint32_t b = 0;
        if (!m.cpu.call(0x8947d310, { block, (uint32_t)id }, xtfw::kStack, &a)) return false;
        if (!m.cpu.call(0x8947d330, { block, (uint32_t)id, 0 }, xtfw::kStack, &b)) return false;
        value = (int)a;
        return a == b;
    }
private:
    static constexpr uint32_t kParts = 0x897a0fc8, kObject = xtfw::kSamp + 0x2000;
    xtfw::Machine m;
};

// One patch's values in the blocks the firmware points at by its own routines, each line's: both stores' routines
// pointing alike, and both getters, against xtPatchValue.  Into n, and wrong those that differ.
void comparePointed(FwGetter& fw, const XtPatchFile& p, long& n, long& wrong, bool first) {
    fw.load(p.store);
    for (const Pointed& b : kPointed)
        for (int line = 0; line < b.lines; ++line) {
            const uint32_t at = fw.point(b.storeOne, line);
            wrong += !at || fw.point(b.storeTwo, line) != at;
            for (int id = b.firstId; id <= b.lastId; ++id) {
                int value = 0;
                ++n;
                const int here = xtPatchValue(p.store, id, line);
                if (at && fw.get(at, id, value) && value == here) continue;
                ++wrong;
                if (first && wrong < 20)
                    std::printf("patch %s id %03x line %d: firmware %d, here %d\n",
                                p.name.c_str(), id, line, value, here);
            }
        }
}

// The same over the disk image's patches, and over a made-up store whose blocks' bytes each differ from the next: the
// presets' knob sets are all 0, PRESET01, so only such a store shows one misplaced.  Prints its line; how many differ.
long comparePointed(const std::vector<XtDiskImage::Entry>& patches, const XtDiskImage& disk, bool first) {
    FwGetter fw;
    long n = 0, wrong = 0, made = 0;
    XtPatchFile p;
    for (const auto& entry : patches) {
        const auto file = disk.read(entry);
        if (xtPatchRead(file.data(), file.size(), p)) comparePointed(fw, p, n, wrong, first);
    }
    p.name = "(made up)";
    for (size_t at = 0x1286; at < p.store.size(); ++at) p.store[at] = (uint8_t)(at * 151 + 7);
    comparePointed(fw, p, made, wrong, first);
    std::printf("pointed: %ld values through the firmware's own pointing routines and getters, and %ld of a made-up "
                "store, %ld different\n", n, made, wrong);
    return wrong;
}
} // namespace

XtPatchCheck xtPatchCheckValues(const XtDiskImage& disk, bool first) {
    XtPatchCheck c;
    const auto patches = disk.list("Internal.prj/Patch");
    std::vector<long> outBy(kXtDescIds, 0);
    XtFwPatchReader fw;
    static constexpr uint32_t kLengths[3] = { 0x146c, 0x14ac, 0x14f2 };   // an older patch's, then "other"
    for (const auto& entry : patches) {
        const auto file = disk.read(entry);
        XtPatchFile p;
        if (!xtPatchRead(file.data(), file.size(), p)) {
            ++c.different;
            continue;
        }
        const bool every = everyEighth(c.patches++);
        ++c.sizes[std::ranges::find(kLengths, be32(&file[0x1c])) - std::begin(kLengths)];
        c.hash = fnv(p.store.data(), p.store.size(), c.hash);
        if (xtPatchName(file.data(), file.size()) == p.name) ++c.named;
        if (every) fw.load(p.store);
        compareValues(c, outBy, p, every ? &fw : nullptr, first);
    }
    for (int id = 0; first && id < kXtDescIds; ++id)
        if (outBy[(size_t)id]) std::printf("out of range: %03x %ld times\n", id, outBy[(size_t)id]);
    long typed = 0, typedDifferent = 0;
    int nth = 0;
    for (const auto& entry : patches) {
        if (!everyEighth(nth++)) continue;
        const auto file = disk.read(entry);
        XtPatchFile p;
        if (!xtPatchRead(file.data(), file.size(), p)) continue;
        fw.load(p.store);
        compareTyped(typed, typedDifferent, p, fw, first);
    }
    std::printf("typed: %ld values against the firmware's read, %ld different\n", typed, typedDifferent);
    c.different += typedDifferent;
    c.different += comparePointed(patches, disk, first);
    return c;
}

void xtPatchCheckPrint(const XtPatchCheck& c) {
    std::printf("patches: %d in the image (lengths 0x146c %d, 0x14ac %d, 0x14f2 %d, other %d), %d names read alike, "
                "%ld values, %ld out of range, %ld against the firmware's reads, %ld different, hash %016llx\n",
                c.patches, c.sizes[0], c.sizes[1], c.sizes[2], c.sizes[3], c.named, c.values, c.outOfRange, c.compared,
                c.different, (unsigned long long)c.hash);
}

namespace {
// The Multi Step Modulator's fields, which an older patch lacks
constexpr int kMsmFirst = 0x48d, kMsmLast = 0x4ef;

// The disk image's patch that sets the most Multi Step Modulator fields away from `blank`'s: how many, its file and store
int mostMsm(const XtDiskImage& disk, const XtPatchFile& blank, std::vector<uint8_t>& file, XtPatchFile& whole) {
    int most = 0;
    for (const auto& entry : disk.list("Internal.prj/Patch")) {
        auto f = disk.read(entry);
        XtPatchFile p;
        if (!xtPatchRead(f.data(), f.size(), p)) continue;
        int uses = 0;
        for (int id = kMsmFirst; id <= kMsmLast; ++id) uses += xtPatchRaw(p.store, id) != xtPatchRaw(blank.store, id);
        if (uses <= most) continue;
        most = uses;
        file = std::move(f);
        whole = std::move(p);
    }
    return most;
}
} // namespace

// An older patch, shorter: a real one that uses the Multi Step Modulator, cut before and after it.  Before, it must come
// out as a patch of no length at all, the firmware's own defaults; after, as the whole file.
int xtPatchCheckShort(const XtDiskImage& disk) {
    int shortBad = 0;
    long shortDefault = 0, shortOwn = 0;
    XtPatchFile blank;
    static constexpr uint8_t kNone[0x20] = { 0, 0x1e, 'S', 'V', 'D', '0', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                             'P', 'A', 'T', 'a', 'K', 'A', 'T', 'E', 0, 0, 0, 0x20, 0, 0, 0, 0 };
    if (!xtPatchRead(kNone, sizeof kNone, blank)) ++shortBad;
    int descMiss = 0;   // those defaults, against the descriptors': they should mostly agree, or the fill did nothing
    for (int id = kMsmFirst; id <= kMsmLast; ++id) {
        const XtParamDesc* d = xtParamDesc(id);
        if (d && d->min != d->max && xtPatchRaw(blank.store, id) != d->def) ++descMiss;
    }
    std::vector<uint8_t> file;
    XtPatchFile whole;
    const int shortUses = mostMsm(disk, blank, file, whole);
    if (!shortUses) ++shortBad;
    else for (const uint32_t len : { 0x14acu, 0x14f2u }) {
        auto cut = file;
        for (int k = 0; k < 4; ++k) cut[0x1c + (size_t)k] = (uint8_t)(len >> (24 - 8 * k));
        XtPatchFile p;
        if (!xtPatchRead(cut.data(), cut.size(), p)) {
            ++shortBad;
            continue;
        }
        const bool own = len == 0x14f2u;
        const XtPatchFile& expected = own ? whole : blank;
        for (int id = kMsmFirst; id <= kMsmLast; ++id) {
            const XtParamDesc* d = xtParamDesc(id);
            if (!d || d->min == d->max) continue;
            if (xtPatchRaw(p.store, id) != xtPatchRaw(expected.store, id)) ++shortBad;
            else ++(own ? shortOwn : shortDefault);
        }
    }
    std::printf("short: a patch using the Multi Step Modulator in %d fields, cut before it %ld values the firmware's "
                "defaults, after it %ld its own, %d not; %d of those defaults aren't the descriptors'\n",
                shortUses, shortDefault, shortOwn, shortBad, descMiss);
    return shortBad;
}
#endif
