// The XT's part on rom.1's kernel in one interpreter (Vc2MachineRig.h, v355).
#include "Vc2MachineRig.h"
#include "Vc2KernelRig.h"
#include "Vc2OsMidi.h"
#include <algorithm>

namespace {
constexpr uint64_t kSteps = 2000000000ull;   // a call's: the prologues, a block's passes
// [FW] rom.1's hardware routines, each answered by a return but the watchdog's (kept); the switch's save and its loads
constexpr uint32_t kTimerSetUp = 0x88011bc0u, kTimerAck = 0x88011be0u;
constexpr uint32_t kWatchdogStart = 0x88011bf0u, kWatchdogStop = 0x88011c10u;
constexpr uint32_t kSaveSr = 0x88010fe0u, kLoads[] = { 0x88010fd8u, 0x88011028u };
// [FW] rom.1's services the rig calls: the kernel's start, a task off its queue, the first dispatch, the watchdog's
// interrupt, TMU0's, a handler's lookup, an event set from an interrupt
constexpr uint32_t kKernelStart = 0x88010ba0u, kUnready = 0x8800fa70u, kFirstDispatch = 0x88010ef0u;
constexpr uint32_t kLoadContext = 0x88011c20u;   // [FW] a task's context loaded, the kernel's start's end
constexpr uint32_t kWatchdogIrq = 0x8800fb60u, kTimerIrq = 0x8800ea30u, kHandlerOf = 0x8800c660u;
// [FW] rom.4's start (its tasks made and ready, then the first dispatch), the DMA's handlers' indices
constexpr uint32_t kRom4Start = 0x89301790u, kInputHandler = 6, kOutputHandler = 7;
constexpr uint32_t kInputIndex = 0x89989cf4u;   // [FW] the input's DMA buffer in use
// [FW] rom.4's main's (0x89300040) calls but the static initializers', which the check's start runs: its pointers to
// the kernel's semaphores and cyclic records (0x8992e208, 0x8992e1e8), before the rows; the idle task's first hook set
// (its handler 12, 0x89300ba0: once every task its start registered (0x89300ab0) waits at the start's barrier
// (0x89300b10, on 0x8000), each is given 0x8000, and the hook becomes 0x89300c00, the system's save to flash once a
// second, the OS's, not run)
constexpr uint32_t kObjects = 0x89301730u, kIdleHookSet = 0x89300b90u, kIdleHook = 12;
// [FW] the MIDI on of the VC-2's front (its 0x8942d6b0's last call): the receive objects on, the hub's node on the MIDI
// IN ring; the hub
constexpr uint32_t kMidiOn = 0x8931a590u, kHub = 0x89961238u;
// [FW] the receive objects of MIDI IN (source 1) and USB MIDI (source 2), and their parser, a byte at a time as the
// receive interrupt gives it (their class's +0x38, 0x893113a0)
constexpr uint32_t kReceives[] = { 0x8995662cu, 0x899598b0u }, kParse = 0x893123b0u;
// [FW] the front's stand-in: entry 12's code (0x89300220), its event, the front's send with a word (0x89314d60) and
// the wait; the message it sends, in Vc2Fw's RAM
constexpr uint32_t kFrontCode = 0x89300220u, kFrontTcb = 0x8994ffccu, kFrontEvent = 1;
constexpr uint32_t kFrontSend = 0x89314d60u, kWaitStub = 0x893017f0u;
constexpr uint32_t kFrontMessageAt = 0x8C1F0020u, kFrontWordAt = 0x8C1F0030u;
// [FW] the queues' sender's wait (0x89310e70) by its caller: the put's (0x89310fb0…) for room, the wait for empty's
// (0x89311070…); a receive object's markers (the ring full's and the USB-MIDI Thru's)
constexpr uint32_t kSenderWait = 0x89310e70u, kPutFrom = 0x89310fb0u, kEmptyFrom = 0x89311070u;
constexpr uint32_t kEmptyTo = 0x893110b0u, kMarker = 0x89311e90u, kMarkerAdded = 0x89311f40u;
// [FW] the OS's own work outside the part, answered 0: its timer queue's post and remove, the MIDI clock out
constexpr uint32_t kOsAnswered[] = { 0x893004a0u, 0x89300640u, 0x89311780u };
// The pages rom.4's and rom.1's drivers touch, as RAM: the DMA controller, TMU, the board's registers, the DMA buffers
// and the page after them (an EXT IN render's last delay slot, 0x893c6330, reads the word after the input's second
// buffer, and never uses it), the SCI MIDI IN's UART is (0xffe00000)
constexpr uint32_t kPages[][2] = { { 0xffa00000u, 0x1000u }, { 0xffd80000u, 0x1000u }, { 0xa4000000u, 0x1000u },
                                   { 0xa4020000u, 0x1000u }, { 0xa4030000u, 0x1000u }, { 0x8cc3f000u, 0x2000u },
                                   { 0xffe00000u, 0x1000u }, { 0xb1000000u, 0x1000u } };
// [FW] rom.1's services the rig counts: an event set from a task and from an interrupt, a task suspended and resumed,
// a semaphore's wait (its +0 the task holding it, 0xffffffff none), a task's restart
constexpr uint32_t kSet = 0x880100a0u, kRaise = 0x88010190u, kSuspend = 0x88010370u, kResume = 0x880103d0u;
constexpr uint32_t kSemaphoreWait = 0x880104e0u, kRestart = 0x88010ae0u, kFree = 0xffffffffu;
constexpr uint32_t kSp = 0x8C1E0000u;   // the idle task's stack, its calls'
constexpr uint8_t kChannelFirst = 0x80, kSystemFirst = 0xf0, kProgram = 0xc0, kPressure = 0xd0, kClassBits = 0xf0;
} // namespace

bool Vc2MachineRig::load(const char* rom4, std::string& why) {
    if (!fw.start(rom4)) {
        why = "no rom.4 at " + std::string(rom4);
        return false;
    }
    if (!vc2LoadKernel(fw.cpu, rom4, own, why)) return false;
    for (const auto& [at, bytes] : kPages) {
        if (fw.cpu.ptr(at, 4) == nullptr) fw.cpu.mapRam(at, bytes);
    }
    for (const uint32_t a : { kTimerSetUp, kTimerAck }) fw.cpu.hook(a, vc2AnswerNothing);
    for (const uint32_t a : kOsAnswered) fw.cpu.hook(a, vc2AnswerNothing);
    for (const uint32_t a : Vc2OsMidiParts::kOsRoutines) fw.cpu.hook(a, vc2AnswerNothing);
    for (const auto [at, on] : { std::pair { kWatchdogStart, true }, std::pair { kWatchdogStop, false } }) {
        fw.cpu.hook(at, [this, on](Sh4& c) {
            watchdog = on;
            c.ret(0);
        });
    }
    fw.cpu.hook(kSaveSr, [](Sh4& c) {   // stc.l sr,@-r15
        c.r[15] -= 4;
        c.wr32(c.r[15], c.sr);
        c.pc = kSaveSr + 2;
    });
    for (const uint32_t a : kLoads) {
        fw.cpu.hook(a, [this](Sh4& c) {   // ldc.l @r15+,spc; ldc.l @r15+,ssr; rte (its slot a nop)
            c.spc = c.rd32(c.r[15]);
            c.ssr = c.rd32(c.r[15] + 4);
            c.r[15] += 8;
            c.sr = c.ssr;
            c.pc = c.spc;
            ++loads;
        });
    }
    countServices();
    frontStandIn();
    if (!fw.cpu.error().empty()) why = fw.cpu.error();
    return fw.cpu.error().empty();
}

// Hooks that count and go on into the service
void Vc2MachineRig::countServices() {
    for (const uint32_t a : { kSet, kRaise }) {
        fw.cpu.hook(a, [this](Sh4& c) { suspendedSets += std::ranges::count(suspended_, c.r[4]); });
    }
    fw.cpu.hook(kSuspend, [this](Sh4& c) {
        ++suspends;
        suspended_.push_back(c.r[4]);
    });
    fw.cpu.hook(kResume, [this](Sh4& c) {
        const auto at = std::ranges::find(suspended_, c.r[4]);
        if (at != suspended_.end()) suspended_.erase(at);
    });
    fw.cpu.hook(kSemaphoreWait, [this](Sh4& c) {
        ++semaphoreWaits;
        const uint32_t holder = c.rd32(c.r[4]);
        heldWaits += holder != kFree && holder != c.rd32(kSystem) ? 1 : 0;
    });
    fw.cpu.hook(kRestart, [this](Sh4&) { ++restarts; });
    fw.cpu.hook(kSenderWait, [this](Sh4& c) {
        roomWaits += c.pr >= kPutFrom && c.pr < kEmptyFrom ? 1 : 0;
        emptyWaits += c.pr >= kEmptyFrom && c.pr < kEmptyTo ? 1 : 0;
    });
    for (const uint32_t a : { kMarker, kMarkerAdded }) {
        fw.cpu.hook(a, [this](Sh4& c) {
            if (c.r[5] < markers.size()) ++markers[c.r[5]];
        });
    }
}

// Entry 12's code a hook: the front's next message sent (0x89314d60, the hub, the message, its word), else its wait,
// each returning to the hook
void Vc2MachineRig::frontStandIn() {
    fw.cpu.hook(kFrontCode, [this](Sh4& c) {
        c.pr = kFrontCode;
        if (front_.empty()) {
            c.r[4] = kFrontEvent;
            c.pc = kWaitStub;
            return;
        }
        const auto [m, w] = front_.front();
        front_.pop_front();
        c.write(kFrontMessageAt, &m, sizeof m);
        c.wr32(kFrontWordAt, w);
        c.r[4] = kHub;
        c.r[5] = kFrontMessageAt;
        c.r[6] = kFrontWordAt;
        c.pc = kFrontSend;
    });
}

std::string Vc2MachineRig::idleCall(uint32_t f, std::initializer_list<uint32_t> args, const char* what) {
    if (!fw.cpu.call(f, args, kSp, nullptr, kSteps)) {
        char at[48];
        std::snprintf(at, sizeof at, " (pc %08x, task %08x)", fw.cpu.pc, word(kSystem));
        return std::string(what) + " faults: " + fw.cpu.error() + at;
    }
    if (word(kSystem) != kIdle) return std::string(what) + " leaves a task running, not the idle";
    return "";
}

std::string Vc2MachineRig::interruptEnd() {
    return watchdog ? idleCall(kWatchdogIrq, {}, "the watchdog's interrupt") : "";
}

std::string Vc2MachineRig::prepare() {
    return fw.cpu.call(kObjects, {}, kSp, nullptr, kSteps) ? "" : "rom.4's main faults: " + fw.cpu.error();
}

// The kernel's start; the application's state on, as rom.1's boot leaves it for rom.4; rom.4's start, its first
// dispatch put off, then its tasks but the rig's and the kernel's own but its idle off their queues, the idle running;
// then the first dispatch, from the idle, the idle's first hook, and the front's MIDI on
std::string Vc2MachineRig::begin() {
    watchdog = false;
    fw.cpu.hook(kLoadContext, vc2AnswerNothing);   // its end, into its first task: back to the rig instead
    const bool kernel = fw.cpu.call(kKernelStart, {}, kSp, nullptr, kSteps);
    fw.cpu.unhook(kLoadContext);
    if (!kernel) return "the kernel's start faults: " + fw.cpu.error();
    fw.cpu.wr32(kAppState, 1);
    if (!fw.cpu.call(kIdleHookSet, {}, kSp, nullptr, kSteps)) return "rom.4's main faults: " + fw.cpu.error();
    bool putOff = false;
    fw.cpu.hook(kFirstDispatch, [&putOff](Sh4& c) {
        putOff = true;
        c.ret(0);
    });
    const bool started = fw.cpu.call(kRom4Start, {}, kSp, nullptr, kSteps);
    fw.cpu.unhook(kFirstDispatch);
    if (!started || !putOff) return "rom.4's start faults: " + fw.cpu.error();
    std::vector<uint32_t> off;
    for (uint32_t e = word(kTableFirst); e <= word(kTableLast); e += kEntryBytes) {
        const uint32_t tcb = word(e + 4);
        if (std::ranges::find(kTasks, tcb) == kTasks.end()) off.push_back(tcb);
    }
    for (const uint32_t tcb : own) {
        if (tcb != kIdle) off.push_back(tcb);
    }
    for (const uint32_t tcb : off) {
        if (!fw.cpu.call(kUnready, { tcb }, kSp, nullptr, kSteps)) return "a task's unready faults: " + fw.cpu.error();
    }
    fw.cpu.wr32(kSystem, kIdle);
    std::string d = idleCall(kFirstDispatch, {}, "the first dispatch");
    if (d.empty()) d = handler(kIdleHook, "the idle's first hook");
    return d.empty() ? idleCall(kMidiOn, { kHub }, "the MIDI on") : d;
}

std::string Vc2MachineRig::tick() {
    const std::string d = idleCall(kTimerIrq, {}, "TMU0's interrupt");
    return d.empty() ? interruptEnd() : d;
}

std::string Vc2MachineRig::handler(int index, const char* what) {
    uint32_t h = 0;
    if (!fw.cpu.call(kHandlerOf, { kHandlers, (uint32_t)index }, kSp, &h, kSteps) || h == 0)
        return std::string(what) + " has no handler";
    const std::string d = idleCall(h, {}, what);
    return d.empty() ? interruptEnd() : d;
}

// The input the DMA has filled: the buffer the handler then gives the audio task (the one not in use until it)
std::string Vc2MachineRig::input(const int32_t* frames) {
    const uint32_t filled = word(kInputIndex) ^ 1u;
    fw.cpu.write(kInputBuffers + (filled << kBufferShift), frames, 1u << kBufferShift);
    return handler(kInputHandler, "the input's interrupt");
}

std::string Vc2MachineRig::output() { return handler(kOutputHandler, "the output's interrupt"); }

// A channel message on source 1 or 2: its bytes, status first, each its receive interrupt's; else the front's
std::string Vc2MachineRig::message(const Vc2MidiMessage& m, uint32_t word) {
    const bool channel = m.status >= kChannelFirst && m.status < kSystemFirst;
    if (channel && (m.source == 1 || m.source == 2)) {
        const uint8_t cls = m.status & kClassBits;
        const uint32_t bytes[] = { m.status, m.data1, m.data2 };
        const size_t count = cls == kProgram || cls == kPressure ? 2 : 3;
        std::string d;
        for (size_t k = 0; k < count && d.empty(); ++k) {
            d = idleCall(kParse, { kReceives[m.source - 1], bytes[k] }, "the receive interrupt");
            if (d.empty()) d = interruptEnd();
        }
        return d;
    }
    front_.emplace_back(m, word);
    const std::string d = idleCall(kRaise, { kFrontTcb, kFrontEvent }, "the front's event");
    return d.empty() ? interruptEnd() : d;
}
