// The SH-4 interpreter's setup (Sh4Interp.h; split from Sh4Interp.cpp in v197, moved as it stood but for loadRom):
// its memory regions, a firmware image's load by its own header, the bulk reads and writes, faults and hooks. The
// paths an instruction takes, the word reads and writes and the executor, stay in Sh4Interp.cpp, where they inline.
#include "Sh4Interp.h"
#include "XtFiles.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>

namespace {
// [FW] where an image loads: its first words jump to 0x89300040, rom.2's and rom.4's alike
constexpr uint32_t kImageBase = 0x89300000u;
// [FW] the image's header, from this file offset: .bss, two words, .bss's end, .data's source and .data, CPU
// addresses (research/tools/vc2rom.cpp; rom.2's are the startup's own, 0x8987cb00, 0x8a01ceb4, 0x89834018, 0x8985858c)
constexpr uint32_t kHeaderAt = 0x1c;
constexpr uint32_t kHeaderBytes = 24;
// An image's region runs to its top rounded to 64 KB (rom.2's to 0x8a020000, as the loader has always mapped it),
// and every section stays below the machines' own RAM (XtFwMachine.h's kRam)
constexpr uint32_t kRegionAlign = 0x10000u;
constexpr uint32_t kImageLimit = 0x8C000000u;

// A firmware image file's bytes into `out`; false, `why` saying so, when it can't be read or is too large for one
bool readImageFile(const char* path, std::vector<uint8_t>& out, std::string& why) {
    std::ifstream f(path ? path : "", std::ios::binary | std::ios::ate);
    if (!f) {
        why = "cannot open rom";
        return false;
    }
    const std::streamoff size = f.tellg();
    if (size <= 0 || size > static_cast<std::streamoff>(kImageLimit - kImageBase)) {
        why = "unexpected rom size";
        return false;
    }
    out.resize(static_cast<size_t>(size));
    f.seekg(0);
    if (f.read(reinterpret_cast<char*>(out.data()), size)) return true;
    why = "cannot read rom";
    return false;
}

// The image's sections by its header; false unless they are laid out as a firmware image's are: .data's source right
// below .data, then .bss, inside the image and below the machines' RAM
bool sectionsOf(const std::vector<uint8_t>& img, Sh4::ImageSections& s) {
    if (img.size() < kHeaderAt + kHeaderBytes) return false;
    const uint8_t* h = img.data() + kHeaderAt;
    s.bss = xtfile::le32(h);
    s.bssEnd = xtfile::le32(h + 12);
    s.dataFrom = xtfile::le32(h + 16);
    s.data = xtfile::le32(h + 20);
    const bool ordered = kImageBase <= s.dataFrom && s.data < s.bss && s.bss < s.bssEnd && s.bssEnd <= kImageLimit;
    return ordered && s.dataFrom == s.data - (s.bss - s.data) && s.data - kImageBase <= img.size();
}
} // namespace

void Sh4::addRegion(uint32_t base, uint32_t size) {
    Region* g = new Region{ phys(base), size, std::vector<uint8_t>(size, 0) };
    regions.push_back(g);
    for (uint32_t o = 0; o < size; o += 1u << kPageBits) page[(g->base + o) >> kPageBits] = g->mem.data() + o;
}

void Sh4::mapRam(uint32_t addr, uint32_t size) {
    addr &= ~((1u << kPageBits) - 1);
    size = (size + (1u << kPageBits) - 1) & ~((1u << kPageBits) - 1);
    addRegion(addr, size);
}

bool Sh4::loadRom(const char* path) {
    std::vector<uint8_t> img;
    if (!readImageFile(path, img, err)) return false;
    ImageSections s;
    if (!sectionsOf(img, s)) {
        err = "not a firmware image: its header's sections are not .data's source, .data, then .bss";
        return false;
    }
    const uint32_t end = std::max<uint32_t>(s.bssEnd, kImageBase + static_cast<uint32_t>(img.size()));
    const uint32_t top = (end + kRegionAlign - 1) & ~(kRegionAlign - 1);
    addRegion(kImageBase, top - kImageBase);
    write(kImageBase, img.data(), img.size());
    // C startup (the entry, 0x89300040): .data from its source, .bss zeroed (the image's bytes past .data aren't RAM's)
    const uint32_t dataBytes = s.bss - s.data, bssBytes = s.bssEnd - s.bss;
    std::memmove(at(s.data, dataBytes), at(s.dataFrom, dataBytes), dataBytes);
    std::memset(at(s.bss, bssBytes), 0, bssBytes);
    image_ = s;
    return true;
}

void Sh4::write(uint32_t a, const void* src, size_t n) {
    const uint8_t* s = (const uint8_t*)src;
    for (size_t i = 0; i < n; ) { uint32_t chunk = (uint32_t)std::min<size_t>(n - i, 4096 - ((a + i) & 4095)); uint8_t* p = at(a + (uint32_t)i, chunk); if (!p) { fault("write", a + (uint32_t)i); return; } std::memcpy(p, s + i, chunk); i += chunk; }
}
void Sh4::read(uint32_t a, void* dst, size_t n) {
    uint8_t* d = (uint8_t*)dst;
    for (size_t i = 0; i < n; ) { uint32_t chunk = (uint32_t)std::min<size_t>(n - i, 4096 - ((a + i) & 4095)); uint8_t* p = at(a + (uint32_t)i, chunk); if (!p) { fault("read", a + (uint32_t)i); std::memset(d + i, 0, n - i); return; } std::memcpy(d + i, p, chunk); i += chunk; }
}

bool Sh4::fault(const char* what, uint32_t addr) {
    if (err.empty()) { char b[96]; std::snprintf(b, sizeof b, "%s at %08x (pc %08x)", what, addr, pc); err = b; }
    return false;
}

void Sh4::hook(uint32_t addr, std::function<void(Sh4&)> fn) {
    hooks[addr] = std::move(fn);
    hookFilter[(addr >> 1) & 0xffff] = 1;
}
