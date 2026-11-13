// rom.4 in the interpreter: vc2rom.h.
#include "vc2rom.h"
#include <algorithm>
#include <cfenv>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

#pragma clang fp contract(off)
#pragma STDC FENV_ACCESS ON

namespace vc2 {

RoundToZero::RoundToZero() : saved(std::fegetround()) { std::fesetround(FE_TOWARDZERO); }
RoundToZero::~RoundToZero() { std::fesetround(saved); }

float fmac(float a, float b, float acc) {
    const float product = a * b;
    return product + acc;
}

namespace {
constexpr uint32_t kBase = 0x89300000;     // [FW] rom.4's load address: its first words jump to 0x89300040, as rom.2's
constexpr uint32_t kHeaderAt = 0x1c;       // [FW] file offset of {.bss, ?, ?, .bss end, .data source, .data}
constexpr uint32_t kPage = 0x1000;

uint32_t word(const std::vector<uint8_t>& img, uint32_t off) {
    uint32_t w = 0;
    std::memcpy(&w, img.data() + off, 4);
    return w;
}
} // namespace

Machine::Machine(const std::string& rom) {
    std::ifstream f(rom, std::ios::binary);
    const std::vector<uint8_t> img((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (img.size() < 0x100) {
        why = "cannot read " + rom;
        return;
    }
    sec.bss = word(img, kHeaderAt);
    sec.bssEnd = word(img, kHeaderAt + 12);
    sec.dataFrom = word(img, kHeaderAt + 16);
    sec.data = word(img, kHeaderAt + 20);
    const bool laidOut = sec.data < sec.bss && sec.bss < sec.bssEnd && sec.dataFrom == sec.data - (sec.bss - sec.data)
                         && sec.data - kBase <= img.size();
    if (!laidOut) {
        why = "the header's sections are not .data then .bss with the source below .data";
        return;
    }
    const uint32_t top = (std::max<uint32_t>(sec.bssEnd, kBase + (uint32_t)img.size()) + kPage - 1) & ~(kPage - 1);
    cpu.mapRam(kBase, top - kBase);
    cpu.write(kBase, img.data(), img.size());
    // C startup: .data from its source, .bss zeroed (the image's bytes past .data are not RAM's)
    std::vector<uint8_t> data(sec.bss - sec.data);
    cpu.read(sec.dataFrom, data.data(), data.size());
    cpu.write(sec.data, data.data(), data.size());
    const std::vector<uint8_t> zero(sec.bssEnd - sec.bss, 0);
    cpu.write(sec.bss, zero.data(), zero.size());
    cpu.mapRam(kRam, kRamSize);
    cpu.hook(kOperatorNew, [this](Sh4& c) { c.ret(alloc(c.r[4])); });
    if (!cpu.error().empty()) why = cpu.error();
}

uint32_t Machine::alloc(uint32_t size) {
    size = (std::max(size, 1u) + 15) & ~15u;
    if (heap + size > kRam + kRamSize) return 0;
    const uint32_t p = heap;
    if (uint8_t* m = cpu.ptr(p, size)) std::memset(m, 0, size);
    heap += size;
    return p;
}

bool Machine::run(uint32_t addr, std::initializer_list<uint32_t> args, uint32_t* r0, uint64_t maxSteps) {
    if (!ok()) return false;
    if (cpu.call(addr, args, kStack, r0, maxSteps)) return true;
    char b[64];
    std::snprintf(b, sizeof b, "call %08x: ", addr);
    std::fprintf(stderr, "%s%s\n", b, cpu.error().c_str());
    return false;
}

float Machine::f32(uint32_t a) {
    const uint32_t u = cpu.rd32(a);
    float f = 0;
    std::memcpy(&f, &u, 4);
    return f;
}

void Machine::setF32(uint32_t a, float f) {
    uint32_t u = 0;
    std::memcpy(&u, &f, 4);
    cpu.wr32(a, u);
}

} // namespace vc2
