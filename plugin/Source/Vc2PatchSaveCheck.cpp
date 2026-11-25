// xt_vc2save <rom.4> [cases] [seed] (v505): rom.4's patch save (Vc2PatchSave.h) held two ways.  Every patch of the disk
// image's Internal.vc2, read by v450's reader (vc2PatchRead, held to rom.4's own read by xt_vc2patch) and written by
// vc2PatchWrite, comes back as its file byte for byte, and that file read again gives its store; and `cases` stores of
// drawn bytes, whose bits no ID holds don't reach a file, each give a file that reads back as a store writing it again.
// And rom.4's setters (vc2PatchSet) on the first factory patch's store: every first-bank ID a section holds and every
// typed ID, a drawn value each (one in four past its range), in zone 1 and again in zone 8 where its section repeats by
// zone and in COSM 2 for a COSM's, each read back by the port's reads (xtStoreValue, vc2StoreTyped): a value in its
// range as it was set, one past it at an end of it (a one-value range isn't held: as set, in its bits), and no byte
// past the ID's own field moved.  So what the setters are handed (each ID's section, its zone's stride, COSM 2's offset,
// its descriptor's place, a typed ID's block and slot) is rom.4's.  A line a part, its first differences under it.
// XT_VC2SAVE_MAIN only.
#ifdef XT_VC2SAVE_MAIN
#include "Vc2PatchCheck.h"
#include "Vc2Params.h"
#include "Vc2PatchFile.h"
#include "Vc2PatchSave.h"
#include "Vc2PatchStore.h"
#include <cstdlib>
#include <random>

namespace {

constexpr int kOtherZone = 7, kPastRange = 4, kPast = 3;   // zone 8; one draw in four past the range, by up to 3

struct IdBytes {
    int at, bytes;
};

// The bytes first-bank ID `id` lies in, in that zone and COSM: its section's place and offset, and its bits' bytes
IdBytes bytesOfId(int id, int zone, int cosm) {
    const XtParamDesc& d = *vc2ParamDesc(id);
    return { xtStoreSection(vc2Store(), id, zone, cosm) + d.offset, (std::abs(d.width) + d.bit + 7) / 8 };
}

// Whether `after` holds `before` in every byte outside the ID's
bool movedOnlyIn(const std::vector<uint8_t>& before, const std::vector<uint8_t>& after, IdBytes f) {
    for (size_t i = 0; i < after.size(); ++i)
        if (after[i] != before[i] && ((int)i < f.at || (int)i >= f.at + f.bytes)) return false;
    return true;
}

bool amongCosmIds(int id) {
    return id >= vc2Store().cosmFirst && id <= vc2Store().cosmLast;
}

// Where an ID is set: zone 1 and COSM 1, and besides zone 8 where its section repeats by zone and COSM 2 for a COSM's
std::vector<XtPatchSet> setPlaces(int id, int value, int field) {
    std::vector<XtPatchSet> at { { id, value, 0, 0, 0 } };
    const bool byZone = xtStoreSection(vc2Store(), field, kOtherZone, 0) != xtStoreSection(vc2Store(), field, 0, 0);
    const bool cosm = amongCosmIds(field);
    if (byZone || cosm) at.push_back({ id, value, byZone ? kOtherZone : 0, cosm ? 1 : 0, 0 });
    return at;
}

struct ValueDraws {
    std::mt19937 rng;
    int in(int lo, int hi) { return lo + (int)(rng() % (unsigned)(hi - lo + 1)); }
    int value(int lo, int hi) { return rng() % kPastRange ? in(lo, hi) : in(lo - kPast, hi + kPast); }
};

// A value set and read back: as set within [lo, hi], at an end of it past it
bool heldToEnds(int read, int set, int lo, int hi) {
    return set >= lo && set <= hi ? read == set : read == lo || read == hi;
}

// One ID set at each of its places on `store` and read back by `read`; `field` the ID whose bytes it takes (a typed
// one's slot)
template <typename Read>
void setAtEach(const std::string& rom4, const std::vector<uint8_t>& store, XtPatchSet s, int field, Read read,
               Vc2PatchTally& t) {
    const auto [lo, hi] = read.range;
    for (const XtPatchSet& at : setPlaces(s.id, s.value, field)) {
        std::vector<uint8_t> after = store;
        std::string why;
        ++t.compared;
        const bool ok = vc2PatchSet(rom4, after, { at }, &why);
        const int got = read(after, at);
        if (ok && read.holds(got, s.value) && movedOnlyIn(store, after, bytesOfId(field, at.zone, at.cosm))) continue;
        char line[160];
        std::snprintf(line, sizeof line, "ID %#x zone %d COSM %d set %d: read %d (range %d to %d) %s", s.id,
                      at.zone + 1, at.cosm + 1, s.value, got, lo, hi, why.c_str());
        t.differs(line);
    }
}

// A first-bank ID's read; a range of one value (min equal to max) isn't held by the setter, so it reads back as set, in
// its field's bits
struct FirstBankRead {
    int id;
    std::pair<int, int> range;
    const XtParamDesc* d;
    int operator()(const std::vector<uint8_t>& st, const XtPatchSet& at) const {
        return xtStoreValue(vc2Store(), st, id, at.zone, at.cosm);
    }
    bool holds(int read, int set) const {
        const int bits = std::abs(d->width);
        const uint32_t mask = bits < 32 ? (1u << bits) - 1 : ~0u;
        if (d->min == d->max) return read == (int)((uint32_t)(set + d->centre) & mask) - d->centre;
        return heldToEnds(read, set, range.first, range.second);
    }
};
struct TypedRead {
    int id;
    std::pair<int, int> range;
    int operator()(const std::vector<uint8_t>& st, const XtPatchSet& at) const {
        return vc2StoreTyped(st, id, at.zone, at.cosm);
    }
    bool holds(int read, int set) const { return heldToEnds(read, set, range.first, range.second); }
};

// Every first-bank ID of the store's sections and every typed ID, set by rom.4's setters and read back
long settersHeld(const std::string& rom4, const std::vector<uint8_t>& store, ValueDraws& r) {
    Vc2PatchTally first { "set, first bank" }, typed { "set, typed" };
    for (const XtPatchSectionDef& s : vc2Store().sections)
        for (int id = s.first; id <= s.last && id < kVc2FirstTyped; ++id)
            if (const XtParamDesc* d = vc2ParamDesc(id)) {
                const std::pair<int, int> range { d->min - d->centre, d->max - d->centre };
                const XtPatchSet set { id, r.value(range.first, range.second), 0, 0, 0 };
                setAtEach(rom4, store, set, id, FirstBankRead { id, range, d }, first);
            }
    for (int id = kVc2FirstTyped; const XtTypedDesc* t = vc2TypedDesc(id); ++id) {
        const int base = vc2StoreSlotBase(id);
        if (base < 0 || !vc2ParamDesc(base + t->slot)) continue;
        const XtPatchSet set { id, r.value(t->min, t->max), 0, 0, 0 };
        setAtEach(rom4, store, set, base + t->slot, TypedRead { id, { t->min, t->max } }, typed);
    }
    return first.say("values by rom.4's setter, read back, no other byte moved")
           + typed.say("typed values by rom.4's setter, read back, no other byte moved");
}

// A store written and its file read back: whether it gave `file` (when one is given) and the store again.  A drawn
// store's bits that no ID holds don't reach the file, so for one (`file` none) the store its file reads back as must
// write the same file again.
bool writtenAndBack(const std::string& rom4, const std::vector<uint8_t>& store, const std::vector<uint8_t>* file,
                    std::string& why) {
    std::vector<uint8_t> written, again;
    Vc2PatchFile back;
    if (!vc2PatchWrite(rom4, store, written, &why)) return false;
    if (file && written != *file) why = "not its file: " + std::to_string(written.size()) + " bytes";
    else if (!vc2PatchRead(written.data(), written.size(), back, &why)) why = "its file unread: " + why;
    else if (file && back.store != store) why = "its file reads back otherwise";
    else if (!file && (!vc2PatchWrite(rom4, back.store, again, &why) || again != written))
        why = "its file read back writes another file " + why;
    return why.empty();
}

// The image's patches and `cases` drawn stores written again; the first factory store, for the setters
long patchesWrittenAgain(const std::string& rom4, int cases, ValueDraws& r, std::vector<uint8_t>& firstStore) {
    XtDiskImage disk;
    Vc2PatchTally files { "saved, the image's patches" }, drawn { "saved, drawn stores" };
    disk.open(rom4.substr(0, rom4.find_last_of('/') + 1) + "V-SynthXTDiskImage.bin.0.bin");
    for (const XtDiskImage::Entry& e : disk.list("Internal.vc2/Patch")) {
        const std::vector<uint8_t> bytes = disk.read(e);
        Vc2PatchFile p;
        std::string why;
        ++files.compared;
        if (!vc2PatchRead(bytes.data(), bytes.size(), p, &why)) files.differs(e.name + ": unread: " + why);
        else if (!writtenAndBack(rom4, p.store, &bytes, why)) files.differs(e.name + ": " + why);
        if (firstStore.empty()) firstStore = p.store;
    }
    for (int c = 0; c < cases; ++c, ++drawn.compared) {
        std::vector<uint8_t> store(kVc2StoreBytes);
        std::string why;
        for (uint8_t& b : store) b = (uint8_t)r.rng();
        if (!writtenAndBack(rom4, store, nullptr, why)) drawn.differs("drawn store " + std::to_string(c) + ": " + why);
    }
    return files.say("back as their files and their stores") + drawn.say("whose files read back write them again")
           + (files.compared ? 0 : 1);
}

} // namespace

int main(int argc, char** argv) {
    Vc2Fw fw;
    const auto run = vc2Start(argc, argv, fw, "xt_vc2save", vc2LoadAll<vc2LoadParams, vc2LoadPatchFiles>);
    if (!run) return 2;
    ValueDraws r { std::mt19937((unsigned)run->seed) };
    std::vector<uint8_t> store;
    long different = patchesWrittenAgain(argv[1], run->cases, r, store);
    if (store.size() == (size_t)kVc2StoreBytes) different += settersHeld(argv[1], store, r);
    else ++different;
    std::printf("xt_vc2save: %ld different\n", different);
    return different ? 1 : 0;
}
#endif
