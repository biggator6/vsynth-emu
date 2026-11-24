// xt_files' check of a patch saved (v602; XtFwPatchFile.h), XT_FILES_MAIN only.  Each of the disk image's patches, read
// into its store and saved by the firmware's own routines, comes back as its file byte for byte, its header too.  And
// values set by the firmware's setters read back through the port's reads (xtPatchValue, xtPatchTyped, held to the
// firmware's own in XtPatchStoreCheck.cpp) as the setter clamps them, no byte past each one's field moved: every ID of
// the first bank, a drawn value each, a quarter of them out of range, and every typed ID of the four blocks' types, in
// zone 1 and COSM 1 and again in zone 8 and COSM 2 where its section repeats so.  So the plumbing (each ID's section,
// its zone's stride, COSM 2's offset, its descriptor, a typed ID's block) is the firmware's.
#ifdef XT_FILES_MAIN
#include "XtFwPatchFile.h"
#include "XtToneCases.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace {
constexpr int kOutOfRange = 4;   // one value in this many drawn past the range, to see the clamp
constexpr uint32_t kSeed = 2024;
// Each block's first slot, by the block the loop walks: the check's own, not xtTypedBaseOf's, which the save uses
constexpr int kTypedBase[] = { kXtMfxSlots, kXtChorusSlots, kXtReverbSlots, kXtCosmSlots };

// What the first bank's read gives after its setter took `v`: the centre added, then unless min is max held to them as
// 0x894818b0 compares, unsigned (so a value under the range, past 0, comes out at its top), the width's bits kept, less
// the centre
int afterSet(const XtParamDesc& d, int v) {
    uint32_t raw = static_cast<uint32_t>(v + d.centre);
    const auto lo = static_cast<uint32_t>(d.min), hi = static_cast<uint32_t>(d.max);
    if (lo != hi) raw = raw < lo ? lo : std::min(raw, hi);
    const int width = std::abs(d.width);
    if (width < 32) raw &= (1u << width) - 1;
    return static_cast<int>(raw) - d.centre;
}

// Whether `after` is `before` but in the bytes ID `id`'s bits lie in (its section, offset, bit and width): some IDs share
// bits (Patch Tempo's 0x11d with 0x11c, the scale tunes with the IDs between them), so an ID either side may read
// otherwise, but no byte past the field may change
bool onlyItsField(const std::vector<uint8_t>& before, const std::vector<uint8_t>& after, const XtParamDesc& d,
                  const XtPatchSet& s) {
    const int at = xtPatchSection(s.id, s.zone, s.cosm) + d.offset, bytes = (std::abs(d.width) + d.bit + 7) / 8;
    for (size_t i = 0; i < after.size(); ++i)
        if ((static_cast<int>(i) < at || static_cast<int>(i) >= at + bytes) && after[i] != before[i]) return false;
    return true;
}

// Where an ID is set: zone 1 (COSM 1), and besides in another zone where its section repeats by zone and in COSM 2
// for a COSM's, so each section's stride and COSM 2's offset are the firmware's too
constexpr int kOtherZone = 7;
std::vector<XtPatchSet> placesOf(int id, int value, bool byZone, bool cosm, int base) {
    std::vector<XtPatchSet> at { { id, value, 0, 0, base } };
    if (byZone || cosm) at.push_back({ id, value, byZone ? kOtherZone : 0, cosm ? 1 : 0, base });
    return at;
}

// The image's patches saved again: how many came back as their files
int savedAgain(const XtDiskImage& disk, bool first, int& patches) {
    int same = 0;
    for (const XtDiskImage::Entry& e : disk.list("Internal.prj/Patch")) {
        const std::vector<uint8_t> bytes = disk.read(e);
        XtPatchFile p;
        if (!xtPatchRead(bytes.data(), bytes.size(), p)) continue;
        ++patches;
        std::vector<uint8_t> file;
        std::string why;
        if (xtFwPatchFile(p.store, file, &why) && file == bytes) {
            ++same;
            continue;
        }
        if (first) std::printf("saved: %s: %s\n", e.name.c_str(), why.empty() ? "not its file" : why.c_str());
    }
    return same;
}

// Every first-bank ID set on `store` and read back, no byte past its field changed; how many came out wrong
int setFirstBank(std::vector<uint8_t> store, Rng& r, bool first, int& set) {
    int wrong = 0;
    for (const XtPatchSectionDef& s : xtPatchSections())
        for (int id = s.first; id <= s.last && id < kXtDescIds; ++id) {
            const XtParamDesc* d = xtParamDesc(id);
            if (!d) continue;
            const int lo = d->min - d->centre, hi = d->max - d->centre;
            const int v = r.one(kOutOfRange) ? r.range(lo - 3, hi + 3) : r.range(lo, hi);
            for (const XtPatchSet& at : placesOf(id, v, s.zoneStride > 0, xtPatchIsCosm(id), 0)) {
                const std::vector<uint8_t> before = store;
                std::string why;
                ++set;
                const bool ok = xtFwPatchSet(store, { at }, &why);
                const int read = xtPatchValue(store, id, at.zone, at.cosm);
                if (ok && read == afterSet(*d, v) && onlyItsField(before, store, *d, at)) continue;
                ++wrong;
                if (first)
                    std::printf("set: ID %#x zone %d COSM %d to %d: read %d, wanted %d %s\n", id, at.zone + 1,
                                at.cosm + 1, v, read, afterSet(*d, v), why.c_str());
            }
        }
    return wrong;
}

// Every typed ID of each block's types set on `store` and read back by xtPatchTyped, clamped to its type's range
int setTypedBank(std::vector<uint8_t> store, Rng& r, bool first, int& set) {
    int wrong = 0;
    for (int block = 0; block < 4; ++block)
        for (int type = 0, lo = 0, hi = 0; xtEffectTypeIds(block, type, lo, hi); ++type)
            for (int id = lo; id <= hi; ++id) {
                const XtTypedDesc* t = xtTypedDesc(id);
                if (!t) continue;
                const int v = r.range(t->min - 2, t->max + 2), base = kTypedBase[block];
                if (xtTypedBaseOf(id) != base) ++wrong;   // and the save's own finding of it
                const bool cosm = base == kXtCosmSlots;   // a COSM's sections repeat by zone and COSM
                for (const XtPatchSet& at : placesOf(id, v, cosm, cosm, base)) {
                    std::string why;
                    ++set;
                    const bool ok = xtFwPatchSet(store, { at }, &why);
                    const int read = xtPatchTyped(store, base, id, at.zone, at.cosm);
                    if (ok && read == std::clamp(v, t->min, t->max)) continue;
                    ++wrong;
                    if (first)
                        std::printf("set: typed ID %#x zone %d COSM %d to %d: read %d %s\n", id, at.zone + 1,
                                    at.cosm + 1, v, read, why.c_str());
                }
            }
    return wrong;
}
} // namespace

int xtPatchSaveCheck(const XtDiskImage& disk, bool first) {
    int patches = 0, set = 0, typedSet = 0;
    const int same = savedAgain(disk, first, patches);
    const auto entries = disk.list("Internal.prj/Patch");
    XtPatchFile p;
    const std::vector<uint8_t> bytes = entries.empty() ? std::vector<uint8_t>() : disk.read(entries.front());
    if (!xtPatchRead(bytes.data(), bytes.size(), p)) {
        std::printf("saved: no patch to set values on\n");
        return 1;
    }
    Rng r { kSeed };
    const int wrong = setFirstBank(p.store, r, first, set), typedWrong = setTypedBank(p.store, r, first, typedSet);
    std::printf("saved: %d of the image's %d patches back as their files; %d values set by the firmware's setter, %d "
                "read otherwise; %d typed, %d read otherwise\n",
                same, patches, set, wrong, typedSet, typedWrong);
    return (same != patches || wrong || typedWrong) ? 1 : 0;
}
#endif
