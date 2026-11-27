// xt_files' older, shorter patch (XT_FILES_MAIN; its main is XtFiles.cpp's): a real patch that uses the Multi Step
// Modulator, cut before and after it, read by the firmware's own routines over its defaults.  Moved from
// XtPatchStoreCheck.cpp as it stood (v473), so that file holds the store's values against the firmware's reads.
#ifdef XT_FILES_MAIN
#include "XtFiles.h"
#include <cstdio>
#include <vector>

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
