// rom.2 and rom.4 side by side: vc2twin.h.
#include "vc2twin.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

namespace vc2 {

Twin::Twin(const char* rom2, Machine& rom4) : vc2(rom4) {
    if (!vsynth.loadRom(rom2) || !vc2.ok()) {
        std::fprintf(stderr, "twin: %s\n", vsynth.error().empty() ? vc2.error().c_str() : vsynth.error().c_str());
        return;
    }
    vsynth.mapRam(kRam, kRamSize);
    ready = true;
}

bool Twin::call(uint32_t a2, uint32_t a4, std::initializer_list<uint32_t> args, float fr4) {
    std::memcpy(&vsynth.fb[0][4], &fr4, 4);
    std::memcpy(&vc2.cpu.fb[0][4], &fr4, 4);
    return callVSynth(a2, args) && vc2.run(a4, args);
}

void Twin::copyToVc2(uint32_t a, uint32_t n) {
    std::vector<uint8_t> x(n);
    vsynth.read(a, x.data(), n);
    vc2.cpu.write(a, x.data(), n);
}

bool Twin::callVSynth(uint32_t a2, std::initializer_list<uint32_t> args) {
    if (vsynth.call(a2, args, kStack)) return true;
    std::fprintf(stderr, "twin: rom.2 %08x: %s\n", a2, vsynth.error().c_str());
    return false;
}

void Twin::hook(Pair p, const std::function<void(Sh4&)>& fn) {
    vsynth.hook(p.vsynth, fn);
    vc2.cpu.hook(p.vc2, fn);
}

void Twin::write(uint32_t a, const void* src, size_t n) {
    vsynth.write(a, src, n);
    vc2.cpu.write(a, src, n);
}

void Twin::writeF32(uint32_t a2, uint32_t a4, float f) {
    uint32_t u = 0;
    std::memcpy(&u, &f, 4);
    vsynth.wr32(a2, u);
    vc2.cpu.wr32(a4, u);
}

long Twin::differs(uint32_t a, uint32_t n, std::span<const Moved> moved) {
    std::vector<uint32_t> x(n / 4), y(n / 4);
    vsynth.read(a, x.data(), n);
    vc2.cpu.read(a, y.data(), n);
    const auto same = [moved](uint32_t v, uint32_t w) {
        return v == w || std::ranges::find_if(moved, [&](const Moved& p) { return p.vsynth == v && p.vc2 == w; })
                             != moved.end();
    };
    const auto [at, _] = std::ranges::mismatch(x, y, same);
    return at == x.end() ? -1 : 4 * (long)(at - x.begin());
}

void Twin::sayDiffers(uint32_t a, long at) {
    const uint32_t w = a + (uint32_t)at;
    std::printf("the images' objects differ at +%#lx (rom.2 %08x, rom.4 %08x); ", at, vsynth.rd32(w), vc2.cpu.rd32(w));
}

int twinRun(Machine& m, int argc, char** argv, bool (*loadTables)(const char*), int (*body)(Twin&)) {
    if (argc < 2) {
        std::fprintf(stderr, "%s <rom.2>\n", argv[0]);
        return 2;
    }
    if (!loadTables(argv[1])) {
        std::fprintf(stderr, "%s: the port's tables don't read from %s\n", argv[0], argv[1]);
        return 1;
    }
    Twin t(argv[1], m);
    if (!t.ok()) return 1;
    const int differ = body(t);
    return differ == 0 ? 0 : 1;
}

} // namespace vc2
