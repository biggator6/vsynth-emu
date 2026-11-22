#pragma once
// An image's effect driver run by hand, from the check's own addresses, store and message bytes, and started by the
// firmware's own task: what xt_fx holds XtFxControl's start (XtFxCheckStart.cpp) and rom.4's messages to
// (XtFxCheckVc2Hand.cpp).  XT_FX_MAIN only.
#ifdef XT_FX_MAIN
#include "XtFxCheck.h"
#include <array>

namespace xtfxcheck {
// [FW] The driver's message: the flags of its second byte, then of its fourth
inline constexpr uint32_t kSystem = 0x20, kAttenuation = 0x10;
inline constexpr uint32_t kPatch = 0x80, kSwitches = 0x40, kMfx = 0x20, kChorus = 0x10, kReverb = 8;

// In a bus log, a wait of so many samples, beside the bytes (address << 8 | value)
inline constexpr uint32_t kWaitMark = 0x80000000;

// [FW] An image's effect task up to its loop: its entry, the wait for events the loop opens with, and the routines on the
// way there that answer nothing here (XtFxCheckHand.cpp says which)
struct XtFxTaskStart {
    uint32_t entry, eventWait;
    std::array<uint32_t, 5> outside;
};
struct HandImage {
    XtFxImage driver;
    XtFxTaskStart task;
};
const HandImage& handRom2();
const HandImage& handRom4();

// The driver as the image's task starts it, then the first message and, where the image has one, the input's routing.
// log: the bus's bytes and waits, from the start on.
struct HandDriver : Rig {
    const XtFxImage& im;
    std::vector<int16_t> store;
    HandDriver(const char* rom, const HandImage& hand, std::vector<uint32_t>* log = nullptr);
    void message(uint32_t second, uint32_t fourth);
    void route();                 // the input's routing: the EXT IN request and its event's routine, where the image has one
    void poll(int sixClocksMs);   // the outside clock: its source at the clock object's +0x40, ms over six clocks at +0x48

private:
    int defaultOf(int id);
    void runTask(const XtFxTaskStart& task);
};
} // namespace xtfxcheck
#endif
