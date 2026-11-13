// vc2run <rom.4> patches <file.p02> …: the VC-2's patches read the project's way, by rom.4's own routines in the interpreter
// (research/VC2_VOCAL_DESIGNER.md, "The factory patches"), as XtFiles.cpp and XtFwPatchReader read a .p00 by rom.2's.  For
// each file: its header checked (0x1e, VC20 at +2, PATaVC_2 at +0x10, the data's offset and length big-endian at +0x18,
// +0x1c); rom.4's default patch (0x89325300) laid out as a file (0x89324b10) under the file's data; rom.4's conversion of a
// file into the store (0x89324b20); then every ID's bits (0x89328580) and value (0x8932b050) by rom.4's descriptors.  Says,
// for every ID its descriptor gives bits: its name (rom.4's table), its range and default, and its values over the files,
// how many are off the default and how many outside the range; then the vocoder block (0x193…0x1a5) file by file.  Fails
// when a file's header is wrong or a routine faults.
// vc2run <rom.4> patches --stores <dir> <file.p02> …: the stores themselves into dir, rom.4's defaults (no file's data
// over them) as default.store and each file's as <its name>.store, for finding where each section of IDs starts.
#include "vc2patches.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string_view>

namespace vc2 {

namespace {

// [FW] rom.4's (rom.2's 0x8947e230, 0x8947dad0, 0x8947dae0, 0x894817b0; kReadValue's is 0x89484310)
constexpr uint32_t kDefaultPatch = 0x89325300, kAsFile = 0x89324b10, kToStore = 0x89324b20;
constexpr uint32_t kReadBits = 0x89328580;
// [FW] rom.4's descriptors (Descriptor's layout) and names
constexpr uint32_t kDescriptors = 0x8950f0b0, kNames = 0x898b5798, kIds = 2476;
constexpr uint32_t kFile = kScratch;
constexpr uint32_t kVocoderFirst = 0x193, kVocoderLast = 0x1a5;
constexpr size_t kHeaderBytes = 0x20, kLongestName = 40;
constexpr uint32_t kStoreBytes = 0x2000;   // what --stores writes: past the file's 0x13a4, as the store's size isn't known

} // namespace

Descriptor descriptorOf(Machine& m, uint32_t id) {
    const uint32_t d = kDescriptors + 12 * id;
    return {m.cpu.rd16(d), (int8_t)m.cpu.rd8(d + 2), m.cpu.rd8(d + 3), m.cpu.rd16(d + 4),
            (int16_t)m.cpu.rd16(d + 6), (int16_t)m.cpu.rd16(d + 8), (int16_t)m.cpu.rd16(d + 10)};
}

TypedDescriptor typedDescriptorOf(Machine& m, uint32_t id) {
    const uint32_t d = kBank + 8 * (id - kFirstTyped);
    return {m.cpu.rd8(d), (int16_t)m.cpu.rd16(d + 2), (int16_t)m.cpu.rd16(d + 4), (int16_t)m.cpu.rd16(d + 6)};
}

std::string paramName(Machine& m, uint32_t id) {
    std::string s;
    for (uint32_t p = m.cpu.rd32(kNames + 4 * id); s.size() < kLongestName; ++p) {
        const char c = (char)m.cpu.rd8(p);
        if (!c) break;
        s += c;
    }
    return s;
}

bool loadStore(Machine& m, const std::vector<uint8_t>& data) {
    if (!m.run(kDefaultPatch, {1}) || !m.run(kAsFile, {kFile})) return false;
    m.cpu.write(kFile, data.data(), data.size());
    return m.run(kToStore, {kPatchStore, kFile});
}

ValueSpread spreadOf(std::vector<int> v, int def, int min, int max) {
    if (v.empty()) return {};
    const auto off = std::ranges::count_if(v, [&](int x) { return x != def; });
    const auto outside = std::ranges::count_if(v, [&](int x) { return x < min || x > max; });
    const auto [lo, hi] = std::ranges::minmax(v);
    std::ranges::sort(v);
    const auto distinct = std::distance(v.begin(), std::unique(v.begin(), v.end()));
    return {lo, hi, (long)distinct, (long)off, (long)outside};
}

bool setStoreValue(Machine& m, uint32_t section, uint32_t id, int value) {
    const Descriptor d = descriptorOf(m, id);
    const int width = d.width < 0 ? -d.width : d.width, total = width + d.bit, bytes = (total + 7) / 8;
    const uint32_t at = section + (uint32_t)d.offset, shift = (uint32_t)(bytes * 8 - total);
    uint32_t v = 0;
    for (int k = 0; k < bytes; ++k) v = v << 8 | m.cpu.rd8(at + (uint32_t)k);
    const uint32_t mask = ((1u << width) - 1) << shift;
    v = (v & ~mask) | (((uint32_t)(value + d.centre) << shift) & mask);
    for (int k = bytes - 1; k >= 0; --k) {
        m.cpu.wr8(at + (uint32_t)k, (uint8_t)v);
        v >>= 8;
    }
    uint32_t back = 0;
    if (!m.run(kReadValue, {section, id}, &back)) return false;
    if ((int32_t)back == value) return true;
    std::printf("store: %03x set to %d reads %d\n", id, value, (int32_t)back);
    return false;
}

namespace {

uint32_t be32(const std::vector<uint8_t>& f, size_t at) {
    return (uint32_t)f[at] << 24 | (uint32_t)f[at + 1] << 16 | (uint32_t)f[at + 2] << 8 | f[at + 3];
}

} // namespace

std::vector<uint8_t> p02Data(const char* path) {
    const std::vector<uint8_t> f = fileContents(path);
    if (f.size() < kHeaderBytes || f[0] || f[1] != 0x1e || std::memcmp(&f[2], "VC20", 4)) return {};
    if (std::memcmp(&f[0x10], "PATaVC_2", 8)) return {};
    const uint32_t at = be32(f, 0x18), bytes = be32(f, 0x1c);
    if ((size_t)at + bytes > f.size()) return {};
    return {f.begin() + at, f.begin() + at + bytes};
}

namespace {

// [FW] the store's sections placed so far, each its IDs and its base (where every one of its IDs reads its default in
// rom.4's default store; research/VC2_VOCAL_DESIGNER.md, "The factory patches"); a read of an ID is at the store + its
// section's base, as rom.2's are (XtPatchStore.cpp's kSections)
struct Section {
    uint32_t first, last, base;
    const char* name;
};
constexpr Section kSections[] = {
    {0x120, 0x1b5, 0x000, "the common"},
    {0x2d3, 0x383, 0x0fe, "the tone's first block (OSC 1 and 2, the VOCODER OSC's with them)"},
};

// Each placed section's IDs' raw bits from the store, by rom.4's reads (and its value, which the bits less the centre
// give); false on a fault
bool readStore(Machine& m, std::vector<int>& raw) {
    for (const Section& s : kSections)
        for (uint32_t id = s.first; id <= s.last; ++id) {
            uint32_t bits = 0, value = 0;
            if (!m.run(kReadBits, {kPatchStore + s.base, kDescriptors + 12 * id}, &bits)) return false;
            if (!m.run(kReadValue, {kPatchStore + s.base, id}, &value)) return false;
            raw[id] = (int)bits;
        }
    return true;
}

// One ID's line: its descriptor, and its values over the files
bool sayId(Machine& m, uint32_t id, const std::vector<std::vector<int>>& raws) {
    const Descriptor d = descriptorOf(m, id);
    std::vector<int> v;
    for (const auto& file : raws) v.push_back(file[id]);
    const ValueSpread s = spreadOf(v, d.def, d.min, d.max);
    std::printf("%03x  %-32s %5d…%-5d %5d | %5d…%-5d %3ld %3ld %3ld\n", id, paramName(m, id).c_str(), d.min, d.max,
                d.def, s.lo, s.hi, s.distinct, s.off, s.outside);
    return s.outside > 0;
}

void sayIds(Machine& m, const std::vector<std::vector<int>>& raws) {
    long outside = 0;
    for (const Section& s : kSections) {
        std::printf("%s, at %#x\nID   name                             range      default", s.name, s.base);
        std::printf(" | over %zu files: lo..hi, values, off the default, outside the range\n", raws.size());
        for (uint32_t id = s.first; id <= s.last; ++id)
            if (descriptorOf(m, id).width != 0) outside += sayId(m, id, raws) ? 1 : 0;
    }
    std::printf("IDs with a value outside the range: %ld\n", outside);
}

void sayVocoder(Machine& m, const std::vector<std::string>& names, const std::vector<std::vector<int>>& raws) {
    std::printf("\nthe vocoder block, file by file (raw values), IDs");
    for (uint32_t id = kVocoderFirst; id <= kVocoderLast; ++id) std::printf(" %03x", id);
    std::printf("\n");
    for (uint32_t id = kVocoderFirst; id <= kVocoderLast; ++id)
        std::printf("  %03x %s\n", id, paramName(m, id).c_str());
    for (size_t f = 0; f < names.size(); ++f) {
        std::printf("%-22s", names[f].c_str());
        for (uint32_t id = kVocoderFirst; id <= kVocoderLast; ++id) std::printf(" %3d", raws[f][id]);
        std::printf("\n");
    }
}

// The store as rom.4 left it into dir/name.store; false when it can't be written
bool writeStore(Machine& m, const std::string& dir, const std::string& name) {
    std::vector<uint8_t> store(kStoreBytes);
    m.cpu.read(kPatchStore, store.data(), store.size());
    const std::string path = dir + "/" + name + ".store";
    FILE* fp = std::fopen(path.c_str(), "wb");
    if (!fp) return false;
    const bool ok = std::fwrite(store.data(), 1, store.size(), fp) == store.size();
    return std::fclose(fp) == 0 && ok;
}

// --stores: the defaults' store and each file's; the program's exit status
int writeStores(Machine& m, const std::string& dir, int argc, char** argv) {
    if (!loadStore(m, {}) || !writeStore(m, dir, "default")) return 1;
    for (int k = 0; k < argc; ++k) {
        const std::vector<uint8_t> data = p02Data(argv[k]);
        const char* base = std::strrchr(argv[k], '/');
        if (data.empty() || !loadStore(m, data) || !writeStore(m, dir, base ? base + 1 : argv[k])) {
            std::printf("%s: not read or not written\n", argv[k]);
            return 1;
        }
    }
    return 0;
}

} // namespace

int patches(Machine& m, int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "%s [--stores <dir>] <file.p02> …\n", argv[0]);
        return 2;
    }
    if (std::string_view(argv[1]) == "--stores" && argc > 2) return writeStores(m, argv[2], argc - 3, argv + 3);
    std::vector<std::string> names;
    std::vector<std::vector<int>> raws;
    for (int k = 1; k < argc; ++k) {
        const std::vector<uint8_t> data = p02Data(argv[k]);
        std::vector<int> raw(kIds);
        if (data.empty()) {
            std::printf("%s: not a .p02\n", argv[k]);
            return 1;
        }
        if (!loadStore(m, data) || !readStore(m, raw)) {
            std::printf("%s: rom.4's routines faulted: %s\n", argv[k], m.error().c_str());
            return 1;
        }
        const char* base = std::strrchr(argv[k], '/');
        names.emplace_back(base ? base + 1 : argv[k]);
        raws.push_back(raw);
    }
    sayIds(m, raws);
    sayVocoder(m, names, raws);
    return 0;
}

} // namespace vc2
