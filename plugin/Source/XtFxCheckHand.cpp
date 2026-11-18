// An image's effect driver by hand: see XtFxCheckHand.h.  XT_FX_MAIN only.
#ifdef XT_FX_MAIN
#include "XtFxCheckHand.h"

namespace xtfxcheck {
namespace {
// [FW] one-off: each image's driver as this check knows it, rom.2's address then rom.4's, written out apart from
// XtFxImage.cpp so that an address wrong there shows; 0 where the image has none to call
struct Pair { uint32_t rom2, rom4; };
using Side = uint32_t Pair::*;

constexpr Pair kSelf { 0x89ced1b4, 0x89980f24 }, kMessageAt { 0x89ced200, 0x89980f84 };
constexpr Pair kClockAt { 0x89979bfc, 0x8995cb40 };
constexpr Pair kLow { 0x897a167c, 0x8950f0b0 }, kHigh { 0x897a56d8, 0x895131a8 };
constexpr Pair kInit { 0x894b4a60, 0x89368280 }, kTake { 0x894ac970, 0x8935fc40 }, kPoll { 0x894acb10, 0x8935fde0 };
constexpr Pair kRequest { 0x894a7dd0, 0 }, kRouting { 0x894aee80, 0 };
constexpr Pair kNap { 0x894b26a0, 0x89365e50 }, kPut { 0x894af400, 0x89362490 }, kGet { 0x894af510, 0x89362610 };
constexpr Pair kCoefficient { 0x894af610, 0x893626d0 }, kOffset { 0x894af6a0, 0x893627f0 };
constexpr Pair kReadA { 0x8947d310, 0x89324350 }, kReadB { 0x8947d330, 0x89324370 };
constexpr Pair kSpin1 { 0x894af3a0, 0x89362430 }, kSpin2 { 0x894af3c0, 0x89362450 };
constexpr Pair kList1 { 0x894d9230, 0x893951b0 }, kList2 { 0x894d90c0, 0x89395040 }, kList3 { 0x894d9020, 0x89394fa0 };
constexpr Pair kEnd { 0x89301680, 0x89301610 }, kPost { 0x894aee70, 0x89391e50 };

// [FW] The task (rom.2's 0x894ad560).  On its way to its loop it calls the driver's start, then its start task, and between
// them what isn't the driver's or isn't run here: two calls to the OS, one not traced that takes two of the task's
// objects, a handler given to the kernel, and the kernel settings' routine, event 0x200's, which XtFxControl doesn't run
constexpr Pair kEntry { 0x894ad560, 0x89360a50 }, kEventWait { 0x89301860, 0x893017f0 };
constexpr Pair kOsFirst { 0x89300ab0, 0x89300ab0 }, kObjects { 0x8946ebb0, 0x893111b0 };
constexpr Pair kHandler { 0x88008240, 0x88008240 }, kSettings { 0x894ace70, 0x89360110 };
constexpr Pair kOsSecond { 0x89300b10, 0x89300b10 };

constexpr HandImage handOn(Side s, int firstHigh, int ids) {
    return {
        .driver = {
            .self = kSelf.*s, .message = kMessageAt.*s, .clock = kClockAt.*s,
            .descLow = kLow.*s, .descHigh = kHigh.*s, .firstHigh = firstHigh, .ids = ids,
            .staticInit = kInit.*s, .takeMessage = kTake.*s, .clockPoll = kPoll.*s,
            .inputRequest = kRequest.*s, .inputEvent = kRouting.*s,
            .sleep = kNap.*s, .putWord = kPut.*s, .getWord = kGet.*s,
            .putCoefficient = kCoefficient.*s, .putOffset = kOffset.*s,
            .reads = { kReadA.*s, kReadB.*s },
            .answered = { kSpin1.*s, kSpin2.*s, kList1.*s, kList2.*s, kList3.*s, kEnd.*s, kPost.*s },
        },
        .task = { kEntry.*s, kEventWait.*s,
                  { kOsFirst.*s, kObjects.*s, kHandler.*s, kSettings.*s, kOsSecond.*s } },
    };
}

constexpr HandImage kRom2 = handOn(&Pair::rom2, 0x55d, 0x8d0);
constexpr HandImage kRom4 = handOn(&Pair::rom4, 0x56a, 0x9ac);
constexpr uint32_t kOutputRate = 44100;
} // namespace

const HandImage& handRom2() { return kRom2; }
const HandImage& handRom4() { return kRom4; }

HandDriver::HandDriver(const char* rom, const HandImage& hand, std::vector<uint32_t>* log)
    : Rig(rom, false, hand.driver), im(hand.driver), store((size_t)im.ids) {
    busLog = log;
    c.mapRam(0, 0x1000);
    for (const uint32_t fn : im.answered)
        if (fn) c.hook(fn, [](Sh4& m) { m.ret(0); });
    for (const uint32_t fn : hand.task.outside) c.hook(fn, [](Sh4& m) { m.ret(0); });
    c.hook(im.sleep, [log](Sh4& m) {
        if (log) log->push_back(kWaitMark | (((m.r[4] & 0xffff) * kOutputRate + 999) / 1000));
        m.ret(0);
    });
    for (int id = 0; id < im.ids; id++) store[(size_t)id] = (int16_t)defaultOf(id);
    for (const uint32_t fn : im.reads)
        c.hook(fn, [this](Sh4& m) {
            const int id = (int16_t)m.r[5];
            m.ret((uint32_t)(int32_t)(id >= 0 && id < im.ids ? store[(size_t)id] : 0));
        });
    call(im.staticInit);
    runTask(hand.task);
    message(kSystem, kPatch);
    if (!im.inputRequest) return;
    call(im.inputRequest);
    call(im.inputEvent);
}

// The task never returns: it's run from its entry until its loop first waits for an event, and left there
void HandDriver::runTask(const XtFxTaskStart& task) {
    uint32_t back = 0;
    c.hook(task.entry, [&back](Sh4& m) { back = m.pr; });
    c.hook(task.eventWait, [&back](Sh4& m) { m.pc = back; });
    call(task.entry);
    c.unhook(task.entry);
    c.unhook(task.eventWait);
}

int HandDriver::defaultOf(int id) {
    if (id >= im.firstHigh) return s16(im.descHigh + 8 * (uint32_t)(id - im.firstHigh) + 2);
    const uint32_t d = im.descLow + 12 * (uint32_t)id;
    return (uint16_t)c.rd16(d + 6) - (uint16_t)c.rd16(d + 4);
}

void HandDriver::message(uint32_t second, uint32_t fourth) {
    c.wr8(im.message + 1, (uint8_t)second);
    c.wr8(im.message + 3, (uint8_t)fourth);
    call(im.takeMessage, { im.self });
}

// one-off: the check's own poll, written apart from XtFxControl::outsideClock, which it's held to
void HandDriver::poll(int sixClocksMs) {
    c.wr8(im.clock + 0x40, 1);
    c.wr16(im.clock + 0x48, (uint16_t)sixClocksMs);
    call(im.clockPoll, { im.self });
}
} // namespace xtfxcheck
#endif
