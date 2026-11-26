// The patch store's layout and the descriptors' reads (from XtFiles.cpp, v187): where each ID's bits are in the store that
// 0x8947dae0 writes, and the firmware's read of them.  HARDWARE_FIRMWARE.md, "V-Synth's files".  Needs xtFwOpen.
#include "XtFiles.h"
#include "XtFirmware.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <mutex>
#include <optional>

namespace {
int u16(const uint8_t* p) { return p[0] | p[1] << 8; }
int16_t s16(const uint8_t* p) { return (int16_t)u16(p); }

// `count` records of `stride` bytes from a CPU address in rom.2 (it maps at 0x89300000), each by `decode`: none with no ROM
// open, or past its end
template <typename T, typename Decode>
std::vector<T> romRecords(uint32_t addr, int count, size_t stride, Decode decode) {
    FILE* f = xtFwRomPath().empty() ? nullptr : std::fopen(xtFwRomPath().c_str(), "rb");
    if (!f) return {};
    std::vector<uint8_t> raw(stride * (size_t)count);
    const bool ok = std::fseek(f, (long)(addr - 0x89300000u), SEEK_SET) == 0
                    && std::fread(raw.data(), 1, raw.size(), f) == raw.size();
    std::fclose(f);
    std::vector<T> table;
    for (size_t at = 0; ok && at + stride <= raw.size(); at += stride) table.push_back(decode(&raw[at]));
    return table;
}

// The process's copy of such a table, read the first time it's asked for with a ROM open and never written after: published
// by an atomic pointer, so a reader holds no lock (the lock keeps two first readers from both loading it).  Empty, and tried
// again on the next call, while no ROM is open or it can't be read.
template <typename T, uint32_t Addr, int Count, size_t Stride, auto Decode>
const std::vector<T>& romTable() {
    static const std::vector<T> none;
    static std::atomic<const std::vector<T>*> slot { nullptr };
    static std::mutex lock;
    if (const auto* t = slot.load(std::memory_order_acquire)) return *t;
    const std::lock_guard<std::mutex> hold(lock);
    if (const auto* t = slot.load(std::memory_order_relaxed)) return *t;
    std::vector<T> loaded = romRecords<T>(Addr, Count, Stride, Decode);
    if (loaded.empty()) return none;
    const auto* t = new std::vector<T>(std::move(loaded));
    slot.store(t, std::memory_order_release);
    return *t;
}

template <typename T> const T* entryAt(const std::vector<T>& t, int at) {
    return at >= 0 && at < (int)t.size() ? &t[(size_t)at] : nullptr;
}

// The second bank: 8 bytes an ID from 0x897a56d8 (xtTypedDescOf)
constexpr int kTypedIds = 0x8d0 + 0x85 - kXtDescIds;   // to the COSMs' last, 0x954
// A block's types (MFX, Chorus, Reverb, COSM), each its first and last ID, an s16 pair
constexpr int kTypes[4] = { 42, 9, 11, 17 };
using IdTable = const std::vector<int16_t>& (*)();
constexpr IdTable kTypeIds[4] = {
    romTable<int16_t, 0x897a8218, 2 * kTypes[0], 2, s16>,
    romTable<int16_t, 0x897a82e4, 2 * kTypes[1], 2, s16>,
    romTable<int16_t, 0x897a8310, 2 * kTypes[2], 2, s16>,
    romTable<int16_t, 0x897cebd8, 2 * kTypes[3], 2, s16>,
};

// The store's layout: 0x8947dae0 writes it.  What a zone has repeats 16 times, and the COSMs twice over that; the arpeggio
// pattern's 16 lines (the manual's "32 steps x 16 lines") are 34 bytes apart, where 0x89479ad0 and 0x89479b20 point a read
// object at line n: +0x128c + 34n, the 16 filling the store up to the Multi Step Modulator's 0x14ac.
constexpr XtPatchSectionDef kSections[] = {
    { 0x103, 0x175, 0, 0 },         // common
    { 0x176, 0x1fb, 84, 0 },        // MFX
    { 0x1fc, 0x22f, 152, 0 },       // Chorus
    { 0x230, 0x292, 180, 0 },       // Reverb
    { 0x293, 0x312, 0xe6, 82 },     // the tone: Structure, MOD, TVA, the OSCs
    { 0x313, 0x3bd, 0x606, 112 },   // the envelopes
    { 0x3be, 0x3e6, 0xd06, 20 },    // the LFOs
    { 0x3e7, 0x42a, 0xe46, 34 },    // the COSMs
    { 0x42b, 0x438, 0x1286, 0 },    // Beam Type, Pad Mode, Pad Hold, the arpeggio
    { 0x439, 0x448, 0x14f2, 0 },    // Zone 1-16 Knob Set, a nibble each: a 0x14fa-byte file's last 8 (0x89479a40, 0x89479a80)
    { 0x449, 0x48c, 0x128c, 34 },   // the arpeggio pattern's line: State (NOTE, CTRL) and Note/CC, then Step 1-32 (v187)
    { 0x48d, 0x4ef, 0x14ac, 0 },    // the Multi Step Modulator
};
constexpr int kCosmFirst = 0x3e7, kCosmLast = 0x42a, kCosmStride = 0x220;
} // namespace

// The descriptors: 12 bytes an ID, u16 byte offset, s8 width (its size is what counts), u8 bit, u16 centre, default, min,
// max (rom.2's from 0x897a167c; rom.4's in the same form, Vc2Params.cpp)
XtParamDesc xtParamDescOf(const uint8_t* p) {
    return { u16(p), (int8_t)p[2], p[3], u16(p + 4), u16(p + 6), u16(p + 8), u16(p + 10) };
}
// The second bank: 8 bytes an ID, u8 slot (a byte to 0x89481d70), s16 default, min, max
XtTypedDesc xtTypedDescOf(const uint8_t* p) { return { p[0], s16(p + 2), s16(p + 4), s16(p + 6) }; }

std::span<const XtPatchSectionDef> xtPatchSections() { return kSections; }
bool xtPatchIsCosm(int id) { return id >= kCosmFirst && id <= kCosmLast; }

const XtParamDesc* xtParamDesc(int id) {
    return entryAt(romTable<XtParamDesc, 0x897a167c, kXtDescIds, 12, xtParamDescOf>(), id);
}
const XtTypedDesc* xtTypedDesc(int id) {
    return entryAt(romTable<XtTypedDesc, 0x897a56d8, kTypedIds, 8, xtTypedDescOf>(), id - kXtDescIds);
}

const XtStoreLayout& xtVSynthStore() {
    static constexpr XtStoreLayout kVSynth { kSections, kCosmFirst, kCosmLast, kCosmStride, xtParamDesc, xtTypedDesc };
    return kVSynth;
}

int xtPatchTyped(const std::vector<uint8_t>& store, int base, int id, int zone, int cosm) {
    return xtStoreTyped(xtVSynthStore(), store, base, id, zone, cosm);
}

int xtEffectTypes(int block) { return block >= 0 && block < 4 ? kTypes[block] : 0; }

bool xtEffectTypeIds(int block, int type, int& first, int& last) {
    if (type < 0 || type >= xtEffectTypes(block)) return false;
    const auto& t = kTypeIds[block]();
    if (t.empty()) return false;
    first = t[2 * (size_t)type];
    last = t[2 * (size_t)type + 1];
    return true;
}

namespace {
// [FW] each block's type ID and first slot, in xtEffectTypeIds' order: MFX, Chorus, Reverb, COSM
struct TypedBlock {
    int typeId, slots;
};
constexpr TypedBlock kTypedBlocks[] = {
    { 0x177, kXtMfxSlots }, { 0x1fc, kXtChorusSlots }, { 0x230, kXtReverbSlots }, { 0x3e8, kXtCosmSlots }
};

// The block whose types have `id`
std::optional<int> typedBlockOf(int id) {
    for (int block = 0; block < static_cast<int>(std::size(kTypedBlocks)); ++block)
        for (int type = 0, lo = 0, hi = 0; xtEffectTypeIds(block, type, lo, hi); ++type)
            if (id >= lo && id <= hi) return block;
    return std::nullopt;
}
} // namespace

int xtTypedBaseOf(int id) {
    const std::optional<int> block = typedBlockOf(id);
    return block ? kTypedBlocks[*block].slots : 0;
}

bool xtTypedInStoredType(const std::vector<uint8_t>& store, int id, int zone, int cosm) {
    const std::optional<int> block = typedBlockOf(id);
    if (!block) return false;
    int lo = 0, hi = 0;
    const int type = xtPatchValue(store, kTypedBlocks[*block].typeId, zone, cosm);
    return xtEffectTypeIds(*block, type, lo, hi) && id >= lo && id <= hi;
}

int xtPatchZones(const std::vector<uint8_t>& store) {
    for (int z = 0; z < 16; ++z)
        if (xtPatchValue(store, 0x124 + z) >= 127) return z + 1;
    return 16;
}

int xtPatchSection(int id, int zone, int cosm) { return xtStoreSection(xtVSynthStore(), id, zone, cosm); }

int xtPatchRaw(const std::vector<uint8_t>& store, int id, int zone, int cosm) {
    return xtStoreRaw(xtVSynthStore(), store, id, zone, cosm);
}

int xtPatchValue(const std::vector<uint8_t>& store, int id, int zone, int cosm) {
    return xtStoreValue(xtVSynthStore(), store, id, zone, cosm);
}

// An image's reads (v450): rom.2's above, rom.4's (Vc2PatchStore.cpp) on its own layout
int xtStoreSection(const XtStoreLayout& l, int id, int zone, int cosm) {
    for (const XtPatchSectionDef& s : l.sections)
        if (id >= s.first && id <= s.last)
            return s.at + s.zoneStride * zone + (id >= l.cosmFirst && id <= l.cosmLast ? l.cosmStride * cosm : 0);
    return -1;
}

int xtStoreRaw(const XtStoreLayout& l, const std::vector<uint8_t>& store, int id, int zone, int cosm) {
    const XtParamDesc* d = l.desc(id);
    const int section = xtStoreSection(l, id, zone, cosm);
    if (!d || section < 0) return 0;
    const int width = std::abs(d->width), total = width + d->bit, bytes = (total + 7) / 8, at = section + d->offset;
    if (at < 0 || at + bytes > (int)store.size()) return 0;
    uint32_t v = 0;
    for (int k = 0; k < bytes; ++k) v = v << 8 | store[(size_t)(at + k)];
    v >>= bytes * 8 - total;
    return (int)(width >= 32 ? v : v & ((1u << width) - 1));   // 0x2c6 and 0x2fa are 32 bits wide
}

void xtStoreRawPut(const XtStoreLayout& l, std::vector<uint8_t>& store, int id, int zone, int cosm, uint32_t raw) {
    const XtParamDesc* d = l.desc(id);
    const int section = xtStoreSection(l, id, zone, cosm);
    if (!d || section < 0) return;
    const int width = std::abs(d->width), total = width + d->bit, bytes = (total + 7) / 8, at = section + d->offset;
    if (at < 0 || at + bytes > (int)store.size() || width > 32) return;
    const int shift = bytes * 8 - total;
    const uint64_t mask = ((uint64_t { 1 } << width) - 1) << shift;
    uint64_t v = 0;
    for (int k = 0; k < bytes; ++k) v = v << 8 | store[(size_t)(at + k)];
    v = (v & ~mask) | ((uint64_t)raw << shift & mask);
    for (int k = bytes - 1; k >= 0; --k, v >>= 8) store[(size_t)(at + k)] = (uint8_t)v;
}

int xtStoreValue(const XtStoreLayout& l, const std::vector<uint8_t>& store, int id, int zone, int cosm) {
    const XtParamDesc* d = l.desc(id);
    if (!d) return 0;
    int v = xtStoreRaw(l, store, id, zone, cosm);
    if (d->min != d->max && (v < d->min || v > d->max)) v = d->def;
    return v - d->centre;
}

int xtStoreTyped(const XtStoreLayout& l, const std::vector<uint8_t>& store, int base, int id, int zone, int cosm) {
    const XtTypedDesc* d = l.typed(id);
    if (!d) return 0;
    const int v = xtStoreValue(l, store, base + d->slot, zone, cosm);
    return v < d->min || v > d->max ? d->def : v;
}
