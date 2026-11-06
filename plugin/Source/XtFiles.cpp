#include "XtFiles.h"
#include "XtFirmware.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

namespace {
uint32_t le32(const uint8_t* p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
uint32_t be32(const uint8_t* p) { return (uint32_t)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]; }
bool fail(std::string* why, const char* what) { if (why) *why = what; return false; }

// The descriptors: 12 bytes an ID from 0x897a167c, u16 byte offset, s8 width (its size is what counts), u8 bit, u16 centre, default, min, max.
constexpr int kDescIds = 0x55d;
constexpr long kDescAt = 0x897a167c - 0x89300000;
const std::vector<XtParamDesc>& descs() {
    static std::mutex lock;
    static std::vector<XtParamDesc> table;
    const std::lock_guard<std::mutex> hold(lock);
    if (!table.empty() || xtFwRomPath().empty()) return table;
    FILE* f = std::fopen(xtFwRomPath().c_str(), "rb");
    if (!f) return table;
    std::vector<uint8_t> raw(12 * (size_t)kDescIds);
    const bool ok = std::fseek(f, kDescAt, SEEK_SET) == 0 && std::fread(raw.data(), 1, raw.size(), f) == raw.size();
    std::fclose(f);
    if (!ok) return table;
    table.resize(kDescIds);
    for (int i = 0; i < kDescIds; ++i) {
        const uint8_t* p = &raw[12 * (size_t)i];
        const auto u16 = [p](int at) { return p[at] | p[at + 1] << 8; };
        table[(size_t)i] = { u16(0), (int8_t)p[2], p[3], u16(4), u16(6), u16(8), u16(10) };
    }
    return table;
}

// The store's layout: 0x8947dae0 writes it.  What a zone has repeats 16 times, and the COSMs twice over that.
struct Section { int first, last, at, zoneStride; };
constexpr Section kSections[] = {
    { 0x103, 0x175, 0, 0 },         // common
    { 0x176, 0x1fb, 84, 0 },        // MFX
    { 0x1fc, 0x22f, 152, 0 },       // Chorus
    { 0x230, 0x292, 180, 0 },       // Reverb
    { 0x293, 0x312, 0xe6, 82 },     // the tone: Structure, MOD, TVA, the OSCs
    { 0x313, 0x3bd, 0x606, 112 },   // the envelopes
    { 0x3be, 0x3e6, 0xd06, 20 },    // the LFOs
    { 0x3e7, 0x42a, 0xe46, 34 },    // the COSMs
    { 0x42b, 0x438, 0x1286, 0 },    // Beam Type, Pad Mode, Pad Hold, the arpeggio
    { 0x48d, 0x4ef, 0x14ac, 0 },    // the Multi Step Modulator
};
constexpr int kCosmFirst = 0x3e7, kCosmLast = 0x42a, kCosmStride = 0x220;
} // namespace

const XtParamDesc* xtParamDesc(int id) {
    const auto& t = descs();
    return id >= 0 && id < (int)t.size() ? &t[(size_t)id] : nullptr;
}

// The second bank: 8 bytes an ID from 0x897a56d8, s16 slot, default, min, max.
const XtTypedDesc* xtTypedDesc(int id) {
    static constexpr int kTypedIds = 0x8d0 + 0x85 - kDescIds;   // to the COSMs' last, 0x954
    static std::mutex lock;
    static std::vector<XtTypedDesc> table;
    const std::lock_guard<std::mutex> hold(lock);
    if (table.empty() && !xtFwRomPath().empty()) {
        if (FILE* f = std::fopen(xtFwRomPath().c_str(), "rb")) {
            std::vector<uint8_t> raw(8 * (size_t)kTypedIds);
            if (std::fseek(f, 0x897a56d8 - 0x89300000, SEEK_SET) == 0 && std::fread(raw.data(), 1, raw.size(), f) == raw.size()) {
                table.resize(kTypedIds);
                for (int i = 0; i < kTypedIds; ++i) {
                    const uint8_t* p = &raw[8 * (size_t)i];
                    const auto s16 = [p](int at) { return (int)(int16_t)(p[at] | p[at + 1] << 8); };
                    table[(size_t)i] = { p[0], s16(2), s16(4), s16(6) };   // the slot is a byte to 0x89481d70
                }
            }
            std::fclose(f);
        }
    }
    const int at = id - kDescIds;
    return at >= 0 && at < (int)table.size() ? &table[(size_t)at] : nullptr;
}

int xtPatchTyped(const std::vector<uint8_t>& store, int base, int id, int zone, int cosm) {
    const XtTypedDesc* d = xtTypedDesc(id);
    if (!d) return 0;
    const int v = xtPatchValue(store, base + d->slot, zone, cosm);
    return v < d->min || v > d->max ? d->def : v;
}

int xtEffectTypes(int block) { return block == 0 ? 42 : block == 1 ? 9 : block == 2 ? 11 : block == 3 ? 17 : 0; }

bool xtEffectTypeIds(int block, int type, int& first, int& last) {
    if (type < 0 || type >= xtEffectTypes(block)) return false;
    static constexpr long kTables[4] = { 0x897a8218 - 0x89300000, 0x897a82e4 - 0x89300000, 0x897a8310 - 0x89300000, 0x897cebd8 - 0x89300000 };
    static std::mutex lock;
    static std::vector<int16_t> tables[4];
    const std::lock_guard<std::mutex> hold(lock);
    if (tables[block].empty()) {
        FILE* f = xtFwRomPath().empty() ? nullptr : std::fopen(xtFwRomPath().c_str(), "rb");
        if (!f) return false;
        std::vector<uint8_t> raw(4 * (size_t)xtEffectTypes(block));
        const bool ok = std::fseek(f, kTables[block], SEEK_SET) == 0 && std::fread(raw.data(), 1, raw.size(), f) == raw.size();
        std::fclose(f);
        if (!ok) return false;
        for (size_t i = 0; i + 1 < raw.size(); i += 2) tables[block].push_back((int16_t)(raw[i] | raw[i + 1] << 8));
    }
    first = tables[block][2 * (size_t)type];
    last = tables[block][2 * (size_t)type + 1];
    return true;
}

int xtPatchZones(const std::vector<uint8_t>& store) {
    for (int z = 0; z < 16; ++z)
        if (xtPatchValue(store, 0x124 + z) >= 127) return z + 1;
    return 16;
}

int xtPatchSection(int id, int zone, int cosm) {
    for (const Section& s : kSections)
        if (id >= s.first && id <= s.last)
            return s.at + s.zoneStride * zone + (id >= kCosmFirst && id <= kCosmLast ? kCosmStride * cosm : 0);
    return -1;
}

int xtPatchRaw(const std::vector<uint8_t>& store, int id, int zone, int cosm) {
    const XtParamDesc* d = xtParamDesc(id);
    const int section = xtPatchSection(id, zone, cosm);
    if (!d || section < 0) return 0;
    const int width = std::abs(d->width), total = width + d->bit, bytes = (total + 7) / 8, at = section + d->offset;
    if (at < 0 || at + bytes > (int)store.size()) return 0;
    uint32_t v = 0;
    for (int k = 0; k < bytes; ++k) v = v << 8 | store[(size_t)(at + k)];
    v >>= bytes * 8 - total;
    return (int)(width >= 32 ? v : v & ((1u << width) - 1));   // 0x2c6 and 0x2fa are 32 bits wide
}

int xtPatchValue(const std::vector<uint8_t>& store, int id, int zone, int cosm) {
    const XtParamDesc* d = xtParamDesc(id);
    if (!d) return 0;
    int v = xtPatchRaw(store, id, zone, cosm);
    if (d->min != d->max && (v < d->min || v > d->max)) v = d->def;
    return v - d->centre;
}

// ── A wave ───────────────────────────────────────────────────────────────────

bool xtWaveRead(const uint8_t* file, size_t size, XtWaveFile& out, std::string* why) {
    out = {};
    const uint8_t* vpfl = nullptr; const uint8_t* vpsc = nullptr; const uint8_t* vplp = nullptr; const uint8_t* vpno = nullptr; const uint8_t* data = nullptr;
    uint32_t nFl = 0, nSc = 0, nLp = 0, nNo = 0, nData = 0;
    // A project's .wav is a RIFF round the same chunks
    size_t i = size >= 12 && !std::memcmp(file, "RIFF", 4) && !std::memcmp(file + 8, "WAVE", 4) ? 12 : 0;
    while (i + 8 <= size && !data) {
        const uint32_t n = le32(file + i + 4);
        if (n > size - i - 8) return fail(why, "a chunk runs past the end");
        const uint8_t* body = file + i + 8;
        if (!std::memcmp(file + i, "vpfl", 4)) { vpfl = body; nFl = n; }
        else if (!std::memcmp(file + i, "vpsc", 4)) { vpsc = body; nSc = n; }
        else if (!std::memcmp(file + i, "vplp", 4)) { vplp = body; nLp = n; }
        else if (!std::memcmp(file + i, "vpno", 4)) { vpno = body; nNo = n; }
        else if (!std::memcmp(file + i, "data", 4)) { data = body; nData = n; }
        i += 8 + (size_t)n + (n & 1);
    }
    out.size = i < size ? i : size;
    // A project's LITE wave is a RIFF with vpsc and vplp but no vpfl, and its samples as they are (Emu_test's 005, 348)
    const bool riff = size >= 12 && !std::memcmp(file, "RIFF", 4);
    if (!vpsc || nSc < 8 || !data || (!vpfl && !riff) || (vpfl && nFl < 44)) return fail(why, "not a V-Synth wave");
    const bool linear = !vpfl || !std::memcmp(vpfl + 40, "LINR", 4);
    if (!linear && std::memcmp(vpfl + 40, "RDAC", 4)) return fail(why, "neither RDAC nor plain samples");
    // 0x8952ee80 checks the data's first two bytes; the next six are nothing the firmware reads.
    if (!linear && (nData < 8 || data[0] != 0 || data[1] != 8)) return fail(why, "an RDAC wave without its header");
    out.encodeType = vpsc[1];
    out.channels = vpsc[2] ? 2 : 1;
    out.frames = (long)le32(vpsc + 4);
    if (nSc >= 32) {
        out.originalKey = (int8_t)vpsc[0xa];
        out.headerCents = (int32_t)le32(vpsc + 0x18);
        out.tempo = (int32_t)le32(vpsc + 0x1c);
    }
    out.loopEnd = out.bwdEnd = out.frames - 1;   // a wave with no loop record: all of it
    if (vplp && nLp >= 28) {
        out.tune = (int32_t)le32(vplp);
        out.loopStart = (long)le32(vplp + 4); out.loopEnd = (long)le32(vplp + 8);
        out.bwdEnd = (long)le32(vplp + 0xc); out.bwdStart = (long)le32(vplp + 0x10);
        out.loopRateFwd = (int16_t)(vplp[0x14] | (vplp[0x15] << 8)); out.loopRateBwd = (int16_t)(vplp[0x16] | (vplp[0x17] << 8));
        out.fwdMode = vplp[0x18]; out.bwdMode = vplp[0x19];
        out.loopSwitchDecides = vplp[0x1a] != 0;
        out.tuneFine = (int8_t)vplp[0x1b];
    }
    if (vpno && nNo >= 12) {
        out.vpnoCount = (int)le32(vpno + 4);
        for (uint32_t k = 12; k + 2 <= nNo; k += 2) out.vpno.push_back((uint16_t)(vpno[k] | vpno[k + 1] << 8));
    }
    if (linear) {
        const size_t want = (size_t)out.frames * (size_t)out.channels;
        if (nData / 2 < want) return fail(why, "fewer samples than the header says");
        out.pcm.resize(want);
        for (size_t k = 0; k < want; ++k) out.pcm[k] = (int16_t)(data[2 * k] | data[2 * k + 1] << 8);
        return true;
    }
    if (!xtFwRdacDecode(data + 8, nData - 8, out.frames, out.channels, out.pcm)) return fail(why, "the firmware's decoder failed (no ROM?)");
    return true;
}

// ── A patch ──────────────────────────────────────────────────────────────────

namespace {
// What 0x89490430 checks: 0x1e, SVD0 at +2, PATaKATE at +0x10; the offset of the data at +0x18 and its length at +0x1c, big-endian.
bool patchData(const uint8_t* file, size_t size, const uint8_t*& data, size_t& bytes) {
    if (size < 0x20 || file[0] != 0 || file[1] != 0x1e || std::memcmp(file + 2, "SVD", 3) || std::memcmp(file + 0x10, "PATaKATE", 8)) return false;
    const size_t at = be32(file + 0x18);
    if (at > size) return false;
    data = file + at;
    bytes = std::min<size_t>({ (size_t)be32(file + 0x1c), size - at, (size_t)kXtPatchBytes });
    return true;
}
} // namespace

bool xtPatchRead(const uint8_t* file, size_t size, XtPatchFile& out, std::string* why) {
    out = {};
    const uint8_t* data = nullptr; size_t bytes = 0;
    if (!patchData(file, size, data, bytes)) return fail(why, "not a V-Synth patch");
    if (!xtFwPatchStore(data, bytes, out.store)) return fail(why, "the firmware's patch routines failed (no ROM?)");
    for (int k = 0; k < 12; ++k) out.name += (char)xtPatchRaw(out.store, 0x103 + k);
    while (!out.name.empty() && out.name.back() == ' ') out.name.pop_back();
    return true;
}

std::string xtPatchName(const uint8_t* file, size_t size) {
    const uint8_t* data = nullptr; size_t bytes = 0;
    if (!patchData(file, size, data, bytes) || bytes < 12) return {};
    std::string name;
    for (int k = 0; k < 12; ++k) {   // 7 bits each from bit 8: the same place in the file's layout as in the store's
        const int bit = 8 + 7 * k, at = bit / 8, shift = bit % 8;
        name += (char)((((data[at] << 8 | data[at + 1]) >> (9 - shift)) & 0x7f));
    }
    while (!name.empty() && name.back() == ' ') name.pop_back();
    return name;
}

// ── The firmware's disk image ────────────────────────────────────────────────

bool XtDiskImage::open(const std::string& path) {
    d.clear();
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    const long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (n > 0) { d.resize((size_t)n); if (std::fread(d.data(), 1, d.size(), f) != d.size()) d.clear(); }
    std::fclose(f);
    if (d.size() < 0x10000) { d.clear(); return false; }
    const auto u16 = [this](size_t at) { return (uint32_t)(d[at] | d[at + 1] << 8); };
    const auto u32 = [this](size_t at) { return le32(&d[at]); };
    const size_t part = 512 * (size_t)u32(446 + 8), boot = part;
    if (boot + 64 > d.size() || u16(boot + 11) != 512 || std::memcmp(&d[boot + 54], "FAT16", 5)) { d.clear(); return false; }
    partitionEnd = part + 512 * (size_t)u32(446 + 12);
    fat = boot + 512 * (size_t)u16(boot + 14);
    root = fat + 512 * (size_t)d[boot + 16] * u16(boot + 22);
    rootBytes = 32 * u16(boot + 17);
    data = root + rootBytes;
    cluster = 512u * d[boot + 13];
    return true;
}

std::vector<uint8_t> XtDiskImage::chain(uint32_t c, uint32_t limit) const {
    std::vector<uint8_t> out;
    while (c >= 2 && c < 0xfff8 && out.size() < limit) {
        const size_t at = data + (size_t)(c - 2) * cluster;
        if (at + cluster > d.size()) break;
        out.insert(out.end(), d.begin() + (long)at, d.begin() + (long)(at + cluster));
        c = (uint32_t)(d[fat + 2 * (size_t)c] | d[fat + 2 * (size_t)c + 1] << 8);
    }
    if (out.size() > limit) out.resize(limit);
    return out;
}

std::vector<XtDiskImage::Entry> XtDiskImage::entries(const std::vector<uint8_t>& raw) const {
    std::vector<Entry> out;
    std::string longName;
    for (size_t i = 0; i + 32 <= raw.size(); i += 32) {
        const uint8_t* x = &raw[i];
        if (x[0] == 0) break;
        if (x[0] == 0xe5) { longName.clear(); continue; }
        if (x[11] == 0x0f) {   // a long name's piece, last first; these names are ASCII
            std::string part;
            for (int k : { 1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30 }) { if (x[k] == 0 || x[k] == 0xff) break; part += (char)x[k]; }
            longName = part + longName;
            continue;
        }
        if (!(x[11] & 8)) {
            std::string name = longName;
            if (name.empty()) {
                for (int k = 0; k < 8 && x[k] != ' '; ++k) name += (char)x[k];
                if (x[8] != ' ') { name += '.'; for (int k = 8; k < 11 && x[k] != ' '; ++k) name += (char)x[k]; }
            }
            if (name != "." && name != "..") out.push_back({ name, (x[11] & 0x10) != 0, (uint32_t)(x[26] | x[27] << 8), le32(x + 28) });
        }
        longName.clear();
    }
    return out;
}

std::vector<XtDiskImage::Entry> XtDiskImage::list(const std::string& folder) const {
    if (d.empty()) return {};
    std::vector<Entry> at = entries(std::vector<uint8_t>(d.begin() + (long)root, d.begin() + (long)(root + rootBytes)));
    size_t from = 0;
    while (from < folder.size()) {
        size_t to = folder.find('/', from);
        if (to == std::string::npos) to = folder.size();
        const std::string step = folder.substr(from, to - from);
        const Entry* next = nullptr;
        for (const Entry& e : at) if (e.isDir && e.name == step) next = &e;
        if (!next) return {};
        at = entries(chain(next->cluster, 0x100000));
        from = to + 1;
    }
    return at;
}

std::vector<uint8_t> XtDiskImage::read(const Entry& file) const { return chain(file.cluster, file.size); }

std::vector<size_t> XtDiskImage::presetWaves() const {
    std::vector<size_t> at;
    for (size_t i = partitionEnd; i + 4 <= d.size(); ++i)
        if (d[i] == 'v' && !std::memcmp(&d[i], "vpfl", 4)) at.push_back(i);
    return at;
}

// ── The check ────────────────────────────────────────────────────────────────
// xt_files <rom>: on the firmware's disk image, beside the ROM: every preset wave against the firmware's whole-wave routines
// (0x89529c20, 0x89529ce0), and every patch's values against the firmware's reads (0x894817b0, 0x89484310).
//   xt_files <rom> wave <file.vpw> [out.raw]    what a wave holds, and its audio
//   xt_files <rom> patch <file.p00> [lo hi]     a patch's name, and the parameters lo..hi (hex) that aren't their defaults
//   xt_files <rom> extract <n> <out.vpw>        the disk image's nth preset wave as a file
#ifdef XT_FILES_MAIN
#include <fstream>
#include <iterator>

static std::vector<uint8_t> slurp(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}
static uint64_t fnv(const void* p, size_t n, uint64_t h = 0xcbf29ce484222325ull) {
    const auto* b = static_cast<const uint8_t*>(p);
    for (size_t i = 0; i < n; ++i) { h ^= b[i]; h *= 0x100000001b3ull; }
    return h;
}

int main(int argc, char** argv) {
    if (argc < 2 || !xtFwOpen(argv[1])) { std::fprintf(stderr, "usage: xt_files rom.2.bin [wave file | patch file [lo hi] | extract n out]\n"); return 2; }
    const std::string mode = argc > 2 ? argv[2] : "";
    if (mode == "wave" && argc > 3) {
        const auto d = slurp(argv[3]);
        XtWaveFile w; std::string why;
        if (!xtWaveRead(d.data(), d.size(), w, &why)) { std::printf("not read: %s\n", why.c_str()); return 1; }
        std::printf("%d ch, %ld frames, encode type %d, loop %ld..%ld\n", w.channels, w.frames, w.encodeType, w.loopStart, w.loopEnd);
        if (argc > 4) std::ofstream(argv[4], std::ios::binary).write((const char*)w.pcm.data(), (std::streamsize)(w.pcm.size() * 2));
        return 0;
    }
    if (mode == "patch" && argc > 3) {
        const auto d = slurp(argv[3]);
        XtPatchFile p; std::string why;
        if (!xtPatchRead(d.data(), d.size(), p, &why)) { std::printf("not read: %s\n", why.c_str()); return 1; }
        std::printf("%s\n", p.name.c_str());
        const int lo = argc > 4 ? (int)std::strtol(argv[4], nullptr, 16) : 0x103, hi = argc > 5 ? (int)std::strtol(argv[5], nullptr, 16) : 0x428;
        for (int id = lo; id <= hi; ++id) {
            const XtParamDesc* e = xtParamDesc(id);
            if (!e || e->min == e->max || xtPatchSection(id) < 0) continue;
            const int v = xtPatchValue(p.store, id);
            if (v != e->def - e->centre) std::printf("%03x %d\n", id, v);
        }
        return 0;
    }
    // The disk image, beside the ROM
    std::string image = argv[1];
    const auto slash = image.find_last_of('/');
    image = (slash == std::string::npos ? std::string() : image.substr(0, slash + 1)) + "V-SynthXTDiskImage.bin.0.bin";
    XtDiskImage diskImage;
    if (!diskImage.open(image)) { std::printf("xt_files: no disk image beside the ROM\n"); return 1; }
    const auto& disk = diskImage.bytes();
    const auto waves = diskImage.presetWaves();
    if (mode == "extract" && argc > 4) {
        const size_t n = (size_t)std::atol(argv[3]);
        if (n >= waves.size()) return 1;
        XtWaveFile w;
        if (!xtWaveRead(&disk[waves[n]], disk.size() - waves[n], w)) return 1;
        std::ofstream(argv[4], std::ios::binary).write((const char*)&disk[waves[n]], (std::streamsize)w.size);
        std::printf("%d ch, %ld frames, encode type %d\n", w.channels, w.frames, w.encodeType);
        return 0;
    }

    const bool first = std::getenv("XT_FILES_FIRST") != nullptr;
    // The waves
    int mono = 0, stereo = 0, unread = 0, waveDifferent = 0; long total = 0; uint64_t hash = 0xcbf29ce484222325ull;
    double edge = 0, inner = 0;
    int types[8] = {};
    int listed = 0, inOrder = 0, listShort = 0, edited[8] = {};     // the waves' vpno
    for (size_t at : waves) {
        XtWaveFile w; std::string why;
        if (!xtWaveRead(&disk[at], disk.size() - at, w, &why)) { ++unread; if (first) std::printf("wave at %zu: %s\n", at, why.c_str()); continue; }
        (w.channels == 2 ? stereo : mono)++;
        total += w.frames;
        if (w.encodeType >= 0 && w.encodeType < 8) ++types[w.encodeType];
        if (w.vpnoCount >= 0) {
            ++listed;
            bool order = (int)w.vpno.size() >= w.vpnoCount;
            for (int k = 0; order && k < w.vpnoCount; ++k) order = w.vpno[(size_t)k] == k;
            inOrder += order;
            if (!order && w.encodeType >= 0 && w.encodeType < 8) ++edited[w.encodeType];
            listShort += (int)w.vpno.size() < w.vpnoCount;
        }
        hash = fnv(w.pcm.data(), w.pcm.size() * 2, hash);
        // Against the whole-wave routine, from the raw chunk
        size_t i = at; const uint8_t* data = nullptr; uint32_t nData = 0;
        while (i + 8 <= disk.size() && !data) { const uint32_t n = le32(&disk[i + 4]); if (!std::memcmp(&disk[i], "data", 4)) { data = &disk[i + 8]; nData = n; } i += 8 + (size_t)n + (n & 1); }
        std::vector<int16_t> whole;
        if (!data || !xtFwRdacWhole(data + 8, nData - 8, w.frames, w.channels, whole) || whole != w.pcm) {
            ++waveDifferent;
            if (first) std::printf("wave at %zu, %d ch: differs from the whole-wave routine\n", at, w.channels);
        }
        // Nothing at the blocks' edges that isn't everywhere: a wrong predictor would show here
        for (size_t k = (size_t)w.channels; k < w.pcm.size(); ++k) {
            const double step = std::abs((int)w.pcm[k] - (int)w.pcm[k - (size_t)w.channels]);
            ((k / (size_t)w.channels) % 16 == 0 ? edge : inner) += step;
        }
    }
    std::printf("rdac: %zu waves (%d mono, %d stereo; %d SOLO, %d BACKING, %d ENSEMBLE), %ld frames, %d unread, %d different from the firmware's whole-wave routines, a block's edge %.3f of the mean step, hash %016llx\n",
                waves.size(), mono, stereo, types[1], types[3], types[5], total, unread, waveDifferent,
                edge * 15.0 / (inner > 0 ? inner : 1.0), (unsigned long long)hash);
    std::printf("vpno: %d waves with a list, %d of them each event in order, the others %d SOLO, %d BACKING, %d ENSEMBLE; %d shorter than their count\n",
                listed, inOrder, edited[1], edited[3], edited[5], listShort);

    // The patches
    const auto patches = diskImage.list("Internal.prj/Patch");
    std::vector<long> outBy(kDescIds, 0);
    XtFwPatchReader fw;
    int nPatches = 0, sizes[4] = {}; long values = 0, compared = 0, different = 0, outOfRange = 0; int named = 0; uint64_t storeHash = 0xcbf29ce484222325ull;
    for (const auto& entry : patches) {
        const auto file = diskImage.read(entry);
        XtPatchFile p;
        if (!xtPatchRead(file.data(), file.size(), p)) { ++different; continue; }
        const int every = nPatches % 8 == 0;   // the firmware's reads on one patch in eight: they're the slow part
        ++nPatches;
        const uint32_t len = be32(&file[0x1c]);
        ++sizes[len == 0x146c ? 0 : len == 0x14ac ? 1 : len == 0x14f2 ? 2 : 3];
        storeHash = fnv(p.store.data(), p.store.size(), storeHash);
        if (xtPatchName(file.data(), file.size()) == p.name) ++named;
        if (every) fw.load(p.store);
        for (const Section& s : kSections)
            for (int id = s.first; id <= s.last; ++id) {
                const XtParamDesc* d = xtParamDesc(id);
                if (!d) continue;
                const bool cosm = id >= kCosmFirst && id <= kCosmLast;
                for (int z = 0; z < (s.zoneStride ? 16 : 1); ++z)
                    for (int c = 0; c < (cosm ? 2 : 1); ++c) {
                        const int raw = xtPatchRaw(p.store, id, z, c), value = xtPatchValue(p.store, id, z, c);
                        ++values;
                        if (id != 0x176 && d->min != d->max && (raw < d->min || raw > d->max)) { ++outOfRange; ++outBy[(size_t)id]; }   // 0x176 lies over the MFX Type, and isn't a patch's
                        if (every) {
                            int fwRaw = 0, fwValue = 0;
                            ++compared;
                            if (!fw.read(xtPatchSection(id, z, c), id, fwRaw, fwValue) || fwRaw != raw || fwValue != value) {
                                ++different;
                                if (first && different < 20) std::printf("patch %s id %03x zone %d cosm %d: raw %d, firmware %d; value %d, firmware %d\n", p.name.c_str(), id, z, c, raw, fwRaw, value, fwValue);
                            }
                        }
                    }
            }
    }
    if (first) for (int id = 0; id < kDescIds; ++id) if (outBy[(size_t)id]) std::printf("out of range: %03x %ld times\n", id, outBy[(size_t)id]);
    // A type's parameters, the slots' meaning under the COSMs' and the effects' types, against the firmware's read of them: every type's IDs on one patch in eight, in range or not
    {
        long typed = 0, typedDifferent = 0;
        int nth = 0;
        for (const auto& entry : patches) {
            if (nth++ % 8) continue;
            const auto file = diskImage.read(entry);
            XtPatchFile p;
            if (!xtPatchRead(file.data(), file.size(), p)) continue;
            fw.load(p.store);
            const auto one = [&](int base, int id, int section, int z, int c) {
                int fwValue = 0;
                ++typed;
                if (!fw.readTyped(section, base, id, fwValue) || fwValue != xtPatchTyped(p.store, base, id, z, c)) {
                    ++typedDifferent;
                    if (first && typedDifferent < 20) std::printf("patch %s base %03x id %03x zone %d cosm %d: firmware %d, here %d\n", p.name.c_str(), base, id, z, c, fwValue, xtPatchTyped(p.store, base, id, z, c));
                }
            };
            for (int z = 0; z < 16; z += 5)
                for (int c = 0; c < 2; ++c)
                    for (int t = 0; t < xtEffectTypes(3); ++t) {
                        int lo = 0, hi = -1;
                        if (!xtEffectTypeIds(3, t, lo, hi)) { ++typedDifferent; continue; }
                        for (int id = lo; id <= hi; ++id) one(kXtCosmSlots, id, xtPatchSection(kXtCosmSlots, z, c), z, c);
                    }
            static constexpr int kBases[3] = { kXtMfxSlots, kXtChorusSlots, kXtReverbSlots };
            for (int b = 0; b < 3; ++b)
                for (int t = 0; t < xtEffectTypes(b); ++t) {
                    int lo = 0, hi = -1;
                    if (!xtEffectTypeIds(b, t, lo, hi)) { ++typedDifferent; continue; }
                    for (int id = lo; id <= hi; ++id) one(kBases[b], id, xtPatchSection(kBases[b]), 0, 0);
                }
        }
        std::printf("typed: %ld values against the firmware's read, %ld different\n", typed, typedDifferent);
        different += typedDifferent;
    }
    // A project's wave: a RIFF round the chunks, its audio plain samples.  And what a wave says of itself, put in its chunks and read back
    {
        int bad = 0;
        XtWaveFile w;
        const size_t at = waves[0];
        if (xtWaveRead(&disk[at], disk.size() - at, w)) {
            std::vector<uint8_t> stream(disk.begin() + (long)at, disk.begin() + (long)(at + w.size));
            const auto chunkAt = [&](const char* tag) { for (size_t i = 0; i + 8 < stream.size(); ++i) if (!std::memcmp(&stream[i], tag, 4)) return i; return (size_t)0; };
            const auto put = [&](size_t a, uint32_t v) { for (int k = 0; k < 4; ++k) stream[a + (size_t)k] = (uint8_t)(v >> (8 * k)); };
            const size_t sc = chunkAt("vpsc") + 8, lp = chunkAt("vplp") + 8, fl = chunkAt("vpfl") + 8, da = chunkAt("data"), no = chunkAt("vpno");
            // the list, edited: three events out of order
            const bool hasList = no && (stream[no + 4] | stream[no + 5] << 8) >= 18;
            if (hasList) { put(no + 8 + 4, 3); stream[no + 8 + 12] = 2; stream[no + 8 + 13] = 0; stream[no + 8 + 14] = 0; stream[no + 8 + 15] = 0; stream[no + 8 + 16] = 1; stream[no + 8 + 17] = 0; }
            if (!hasList) ++bad;
            stream[sc + 0xa] = 65; put(sc + 0x18, 15); put(sc + 0x1c, 999991);
            put(lp, (uint32_t)-1234); put(lp + 4, 11); put(lp + 8, 2222); put(lp + 0xc, 333); put(lp + 0x10, 44);
            stream[lp + 0x18] = 2; stream[lp + 0x19] = 0; stream[lp + 0x1a] = 0; stream[lp + 0x1b] = (uint8_t)(int8_t)-7;
            stream[lp + 0x14] = 0x2c; stream[lp + 0x15] = 0x01; stream[lp + 0x16] = 0xa9; stream[lp + 0x17] = 0xfe;   // 300 and -343
            XtWaveFile back;
            const auto same = [&](const XtWaveFile& b) {
                return b.originalKey == 65 && b.headerCents == 15 && b.tempo == 999991 && b.tune == -1234 && b.loopStart == 11 && b.loopEnd == 2222 && b.bwdEnd == 333 && b.bwdStart == 44
                    && b.fwdMode == 2 && b.bwdMode == 0 && !b.loopSwitchDecides && b.tuneFine == -7 && b.loopRateFwd == 300 && b.loopRateBwd == -343 && b.frames == w.frames && b.channels == w.channels && b.encodeType == w.encodeType
                    && b.vpnoCount == 3 && b.vpno.size() >= 3 && b.vpno[0] == 2 && b.vpno[1] == 0 && b.vpno[2] == 1;
            };
            if (!xtWaveRead(stream.data(), stream.size(), back) || !same(back) || back.pcm != w.pcm) ++bad;
            // The RIFF: the chunks up to data, LINR for RDAC, and the samples
            std::vector<uint8_t> riff = { 'R','I','F','F',0,0,0,0,'W','A','V','E','f','m','t',' ',16,0,0,0, 1,0,(uint8_t)w.channels,0, 0x44,0xac,0,0, 0,0,0,0, 4,0,16,0 };
            std::vector<uint8_t> head(stream.begin(), stream.begin() + (long)da);
            std::memcpy(&head[fl + 40], "LINR", 4);
            riff.insert(riff.end(), head.begin(), head.end());
            const uint32_t bytes = (uint32_t)w.pcm.size() * 2;
            const uint8_t tag[8] = { 'd','a','t','a',(uint8_t)bytes,(uint8_t)(bytes >> 8),(uint8_t)(bytes >> 16),(uint8_t)(bytes >> 24) };
            riff.insert(riff.end(), tag, tag + 8);
            for (int16_t x : w.pcm) { riff.push_back((uint8_t)x); riff.push_back((uint8_t)((uint16_t)x >> 8)); }
            if (!xtWaveRead(riff.data(), riff.size(), back) || !same(back) || back.pcm != w.pcm) ++bad;
            // A project's LITE wave: the fmt chunk, vpsc with type 0 and vplp, the samples; no vpfl
            {
                std::vector<uint8_t> lite(riff.begin(), riff.begin() + 36);
                for (const char* t : { "vpsc", "vplp" }) {
                    const size_t c = chunkAt(t);
                    const uint32_t n = (uint32_t)stream[c + 4] | (uint32_t)stream[c + 5] << 8 | (uint32_t)stream[c + 6] << 16 | (uint32_t)stream[c + 7] << 24;
                    lite.insert(lite.end(), stream.begin() + (long)c, stream.begin() + (long)(c + 8 + n + (n & 1)));
                }
                lite[36 + 8 + 1] = 0;                              // vpsc +1: LITE
                lite.insert(lite.end(), tag, tag + 8);
                for (int16_t x : w.pcm) { lite.push_back((uint8_t)x); lite.push_back((uint8_t)((uint16_t)x >> 8)); }
                XtWaveFile l;
                if (!xtWaveRead(lite.data(), lite.size(), l) || l.encodeType != 0 || l.pcm != w.pcm || l.loopEnd != 2222 || l.tune != -1234 || l.frames != w.frames || l.vpnoCount != -1 || !l.vpno.empty()) ++bad;
                // the same with no vpsc is a plain WAV
                std::vector<uint8_t> noSc(lite);
                std::memcpy(&noSc[36], "junk", 4);
                if (xtWaveRead(noSc.data(), noSc.size(), l)) ++bad;
            }
            // Plain WAVs aren't this, nor is a wave cut short
            XtWaveFile none;
            const std::vector<uint8_t> plain = { 'R','I','F','F',36,0,0,0,'W','A','V','E','f','m','t',' ',16,0,0,0, 1,0,1,0, 0x44,0xac,0,0, 0,0,0,0, 2,0,16,0, 'd','a','t','a',0,0,0,0 };
            if (xtWaveRead(plain.data(), plain.size(), none)) ++bad;
            std::vector<uint8_t> shortRiff(riff.begin(), riff.end() - 4);
            std::memcpy(&shortRiff[shortRiff.size() - bytes + 4 - 8], tag, 8);   // the same header
            const uint32_t less = bytes - 4;
            for (int k = 0; k < 4; ++k) shortRiff[shortRiff.size() - less - 4 + (size_t)k] = (uint8_t)(less >> (8 * k));
            if (xtWaveRead(shortRiff.data(), shortRiff.size(), none)) ++bad;
        } else ++bad;
        std::printf("a wave's own: %d wrong\n", bad);
        different += bad;
    }
    std::printf("patches: %d in the image (lengths 0x146c %d, 0x14ac %d, 0x14f2 %d, other %d), %d names read alike, %ld values, %ld out of range, %ld against the firmware's reads, %ld different, hash %016llx\n",
                nPatches, sizes[0], sizes[1], sizes[2], sizes[3], named, values, outOfRange, compared, different, (unsigned long long)storeHash);
    // An older patch, shorter: a real one that uses the Multi Step Modulator, cut before and after it.  Before, it must come out as a
    // patch of no length at all, the firmware's own defaults; after, as the whole file.
    int shortBad = 0, shortUses = 0; long shortDefault = 0, shortOwn = 0;
    XtPatchFile blank;
    { const uint8_t none[0x20] = { 0, 0x1e, 'S', 'V', 'D', '0', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 'P', 'A', 'T', 'a', 'K', 'A', 'T', 'E', 0, 0, 0, 0x20, 0, 0, 0, 0 };
      if (!xtPatchRead(none, sizeof none, blank)) ++shortBad; }
    int descMiss = 0;
    for (int id = 0x48d; id <= 0x4ef; ++id) {   // those defaults, against the descriptors': they should mostly agree, or the fill did nothing
        const XtParamDesc* d = xtParamDesc(id);
        if (d && d->min != d->max && xtPatchRaw(blank.store, id) != d->def) ++descMiss;
    }
    std::vector<uint8_t> file; XtPatchFile whole;
    for (const auto& entry : patches) {
        auto f = diskImage.read(entry);
        XtPatchFile p;
        if (!xtPatchRead(f.data(), f.size(), p)) continue;
        int uses = 0;
        for (int id = 0x48d; id <= 0x4ef; ++id) if (xtPatchRaw(p.store, id) != xtPatchRaw(blank.store, id)) ++uses;
        if (uses > shortUses) { shortUses = uses; file = std::move(f); whole = std::move(p); }
    }
    if (!shortUses) ++shortBad;
    else for (const uint32_t len : { 0x14acu, 0x14f2u }) {
        auto cut = file;
        cut[0x1c] = (uint8_t)(len >> 24); cut[0x1d] = (uint8_t)(len >> 16); cut[0x1e] = (uint8_t)(len >> 8); cut[0x1f] = (uint8_t)len;
        XtPatchFile p;
        if (!xtPatchRead(cut.data(), cut.size(), p)) { ++shortBad; continue; }
        for (int id = 0x48d; id <= 0x4ef; ++id) {
            const XtParamDesc* d = xtParamDesc(id);
            if (!d || d->min == d->max) continue;
            const bool own = len == 0x14f2u;
            if (xtPatchRaw(p.store, id) == xtPatchRaw((own ? whole : blank).store, id)) ++(own ? shortOwn : shortDefault); else ++shortBad;
        }
    }
    std::printf("short: a patch using the Multi Step Modulator in %d fields, cut before it %ld values the firmware's defaults, after it %ld its own, %d not; %d of those defaults aren't the descriptors'\n", shortUses, shortDefault, shortOwn, shortBad, descMiss);
    return (waveDifferent || unread || different || shortBad) ? 1 : 0;
}
#endif
