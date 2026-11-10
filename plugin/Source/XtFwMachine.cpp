// The machine XtFirmware's routines run on: XtFwMachine.h.
#include "XtFwMachine.h"
#include <algorithm>
#include <atomic>
#include <cstring>

namespace xtfw {

namespace {
// rom.2's OS calls the machine answers (see research/HARDWARE_FIRMWARE.md)
constexpr uint32_t kMalloc = 0x89527910, kCalloc = 0x89527930, kFree = 0x89527950;
constexpr uint32_t kOsInit = 0x895278f0, kUi = 0x89511360;
std::atomic<bool> nearestAll { false };   // roundNearest's, set before a harness's machines are made
} // namespace

Machine::Machine(const std::string& rom, std::size_t samples) {
    cpu.stepLimit = kCallSteps + kStepsPerSample * samples;
    if (!cpu.loadRom(rom.c_str())) return;
    cpu.mapRam(kRam, kRamSize);
    cpu.forceNearest = nearestAll.load(std::memory_order_relaxed);
    auto alloc = [this](uint32_t sz) -> uint32_t {
        // malloc(0) (the SOLO encoder's pool under 160 samples) still gives a block
        sz = (std::max(sz, 1u) + 15) & ~15u;
        const uint32_t p = heap;
        if (heap + sz > kRam + kRamSize) return 0;
        if (uint8_t* m = cpu.ptr(p, sz)) std::memset(m, 0, sz);
        heap += sz;
        return p;
    };
    cpu.hook(kMalloc, [alloc](Sh4& c) { c.ret(alloc(c.r[4])); });
    cpu.hook(kCalloc, [alloc](Sh4& c) { c.ret(alloc(c.r[4] * std::max(1u, c.r[5]))); });
    cpu.hook(kFree, [](Sh4& c) { c.ret(0); });
    cpu.hook(kOsInit, [](Sh4& c) { c.ret(0); });
    cpu.hook(kUi, [](Sh4& c) { c.ret(0); });
    ok = true;
}

std::unique_ptr<Machine> waveMachine(const std::string& rom, const int16_t* pcm, long frames, int channels,
                                     std::size_t reserve) {
    if (rom.empty() || !pcm || frames <= 0 || (channels != 1 && channels != 2)) return nullptr;
    const std::size_t samples = (std::size_t)frames * (std::size_t)channels;
    if (kSamp + samples * 2 + reserve > kHeap) return nullptr;
    auto M = std::make_unique<Machine>(rom, samples);
    if (!M->ok) return nullptr;
    M->cpu.write(kSamp, pcm, samples * 2);
    return M;
}

void roundNearest(bool on) {
    nearestAll.store(on, std::memory_order_relaxed);
}

} // namespace xtfw
