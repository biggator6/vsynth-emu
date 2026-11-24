// rom.1, the kernel, in the interpreter for the VC-2's checks (Vc2KernelRig.h, v353).
#include "Vc2KernelRig.h"
#include <fstream>
#include <iterator>

namespace {
constexpr uint32_t kSteps = 200000;   // a service's: a few hundred instructions; the start's paint of its stacks more
// [FW] rom.1's C start-up's words (0x88001054…0x8800107c): .data's source, its end and its copy; the .bss areas
constexpr uint32_t kDataFrom = 0x8800106cu, kDataFromEnd = 0x88001070u, kDataTo = 0x88001074u;
constexpr uint32_t kBss[][2] = { { 0x88001054u, 0x88001058u }, { 0x8800105cu, 0x88001060u },
                                 { 0x88001064u, 0x88001068u } };
constexpr uint32_t kVbrWord = 0x8800102cu;   // [FW] the word its reset code sets VBR to: rom.1's own base
// Its RAM as its sections lay it out: the image, .data and .bss to 0x88189900 and the uncached area at 0xa81c0000
// (0x881c0000 by its physical address) in the first 2 MB; the stacks' and heap's megabyte below rom.4's image
constexpr uint32_t kKernelRam = 0x00200000u, kHeap = 0x89200000u, kHeapBytes = 0x00100000u;
// [FW] the hardware the services touch, each answered by a return: TMU0's set-up and acknowledgement, the
// watchdog's start and stop; and the context's load at the kernel's start
constexpr uint32_t kTimerSetUp = 0x88011bc0u, kTimerAck = 0x88011be0u, kWatchdogStart = 0x88011bf0u;
// The switch (0x88011c30) is given the two TCBs; it adds tcbSP's 0x2c to each itself
constexpr uint32_t kWatchdogStop = 0x88011c10u, kLoadContext = 0x88011c20u, kSwitch = 0x88011c30u;

void answerNothing(Sh4& c) { c.ret(0); }
} // namespace

int Vc2KernelRig::task(uint32_t a) {
    if (a == 0) return -1;
    if (a < kTcbs || a >= kEntries || (a - kTcbs) % kTcbBytes != 0) return -2;
    return (int)((a - kTcbs) / kTcbBytes);
}

bool Vc2KernelRig::call(uint32_t f, std::initializer_list<uint32_t> args, uint32_t* r0) {
    uint32_t r = 0;
    const bool ok = cpu.call(f, args, kSp, &r, kSteps);
    if (r0) *r0 = r;
    return ok;
}

bool Vc2KernelRig::load(const char* rom4, std::string& why) {
    std::string path = rom4;
    const size_t at = path.rfind(".rom.4.");
    if (at == std::string::npos) {
        why = "rom.1 is found beside rom.4, named as the unpacker leaves it (V-SynthXT.rom.4.bin)";
        return false;
    }
    path.replace(at, 7, ".rom.1.");
    std::ifstream f(path, std::ios::binary);
    const std::vector<uint8_t> img((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (img.empty() || img.size() >= kKernelRam) {
        why = "no rom.1 at " + path;
        return false;
    }
    cpu.mapRam(kBase, kKernelRam);
    cpu.mapRam(kHeap, kHeapBytes);
    cpu.mapRam(kRam, kRamBytes);
    cpu.write(kBase, img.data(), img.size());
    if (cpu.rd32(kVbrWord) != kBase) {
        why = path + " isn't rom.1: its reset code doesn't set VBR to 0x88000000";
        return false;
    }
    std::vector<uint8_t> data(cpu.rd32(kDataFromEnd) - cpu.rd32(kDataFrom));
    cpu.read(cpu.rd32(kDataFrom), data.data(), data.size());
    cpu.write(cpu.rd32(kDataTo), data.data(), data.size());
    for (const auto& [from, to] : kBss)
        for (uint32_t a = cpu.rd32(from); a < cpu.rd32(to); a += 4) cpu.wr32(a, 0);
    for (uint32_t e = kOwnTasks; e < kOwnTasksEnd; e += 20) own.push_back(cpu.rd32(e + 4));
    for (const uint32_t a : { kTimerSetUp, kTimerAck, kLoadContext }) cpu.hook(a, answerNothing);
    cpu.hook(kWatchdogStart, [this](Sh4& c) {
        watchdog = true;
        c.ret(0);
    });
    cpu.hook(kWatchdogStop, [this](Sh4& c) {
        watchdog = false;
        c.ret(0);
    });
    cpu.hook(kSwitch, [this](Sh4& c) {   // the tail that follows takes the events; what it takes is read first
        const uint32_t to = c.r[5];
        switches.emplace_back(c.r[4], to);
        answer = c.rd16(to) & c.rd16(to + 2);
        c.ret(0);
    });
    if (!cpu.error().empty()) why = cpu.error();
    return cpu.error().empty();
}

bool Vc2KernelRig::begin(const std::vector<std::pair<int, uint8_t>>& tasks) {
    watchdog = false;   // stopped, as at a reset
    bool ok = call(kStart, {});
    for (const uint32_t t : own) ok = ok && call(kUnready, { t });
    for (size_t t = 0; t < tasks.size(); ++t) {   // each entry as rom.4's: code, TCB, ready queue, quantum, stack
        const uint32_t entry = kEntries + 20u * (uint32_t)t, stack = kStacks + kStackBytes * (uint32_t)t;
        const uint32_t record = kStacks - 0x100u + 12u * (uint32_t)t;
        const uint32_t words[5] = { handler(0xff), tcb((int)t), kReadyQueue + 4u * (uint32_t)tasks[t].first,
                                    tasks[t].second, record };
        cpu.write(entry, words, sizeof words);
        cpu.wr32(record, stack + kStackBytes - 4);
        cpu.wr32(record + 4, stack);
        cpu.wr32(tcb((int)t) + 0x10, entry);
        ok = ok && call(kReset, { tcb((int)t) });
    }
    for (int t = (int)tasks.size() - 1; t >= 0; --t) ok = ok && call(kMakeReady, { tcb(t) });
    switches.clear();
    return ok && call(kFirstDispatch, {});
}
