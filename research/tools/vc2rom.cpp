// rom.4 in the interpreter: vc2rom.h.
#include "vc2rom.h"
#include <algorithm>
#include <cfenv>
#include <cstdio>
#include <cstring>

#pragma clang fp contract(off)
#pragma STDC FENV_ACCESS ON

namespace vc2 {

RoundToZero::RoundToZero() : saved(std::fegetround()) { std::fesetround(FE_TOWARDZERO); }
RoundToZero::~RoundToZero() { std::fesetround(saved); }

float fmac(float a, float b, float acc) {
    const float product = a * b;
    return product + acc;
}

// rom.4 laid out by its own header, as Sh4::loadRom lays out any image (v197)
Machine::Machine(const std::string& rom) {
    if (!cpu.loadRom(rom.c_str())) {
        why = cpu.error();
        return;
    }
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
