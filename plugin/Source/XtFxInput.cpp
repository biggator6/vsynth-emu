// The outside input's way in through the effect chip (v204; HARDWARE_FIRMWARE.md, "The input's way in"): the firmware's
// routing of it by External Input Type and Source, and XtFxUnit's input slots and what the chip hands back of it.
#include "Sh4Interp.h"
#include "XtFxChip.h"
#include "XtFxControl.h"

namespace {
// [FW] program A's slots (VC2_VOCAL_DESIGNER.md, "Program A's routing"): the converter's input L, R at 0x10, 0x12 (analog,
// MIC/INPUT) and 0x18, 0x1a (USB), each read the sample after, as the frame's at 0x20 are; the outside input's copies for
// the SH-4 at 0x98, 0x9a, which read one on (0x99, 0x9b) once the chip has rotated
constexpr unsigned kAnalogL = 0x10, kAnalogR = 0x12, kUsbL = 0x18, kUsbR = 0x1a, kBackL = 0x99, kBackR = 0x9b;
constexpr int kBlockFrames = 32;   // the most a process() takes, the bus's block
}

// The EXT IN request (rom.2's 0x894a7dd0): External Input Type (ID 0x78) to the routing's wanted byte 6 (0x89ced225 + 6),
// Source (0x7c) to byte 7, bytes 1, 2 and 5 to 0, and event 0x40 posted (0x894aee70, hooked: this is the event).  Then the
// event's routine (0x894aee80), which writes what differs from what it last applied (0x89ced22d), with its fades.  At the
// start all of it differs, as the driver's start (0x894ae960) leaves 0x80 applied.  What calls the request on the XT isn't
// found; the plugin's input is EXT IN's alone, so the start asks, and each change of the two.
// rom.4's (0x8935b050, v400): External In Type (0x7a) to byte 6 (0x89980fa9 + 6), bytes 1 and 5 to 0, no Source; its
// event (0x893620e0) stores the Type among what it applied (0x89980fb0 + 6) and sends the chip nothing for it.  The
// effect task runs it on a message 0x40 with code 4 (0x8935b090, a vtable's) and the sampling's 0x89359e60, as copies.
void XtFxControl::routeInput() {
    if (!image_.inputRequest || !call(image_.inputRequest)) return;   // an image with no routing mapped asks for none
    call(image_.inputEvent);
    flush(0);
}

uint8_t XtFxControl::driverByte(uint32_t address) const { return sh4_->rd8(address); }

void XtFxUnit::pushInput(const int32_t* lr, int frames) { in_.push(lr, frames); }

// Frame i's input into both pairs of slots, so that either Source hears the plugin's one input; the chip has a word's top
// 24 bits, as the frame's way in has (toChip).  An image that adds its USB pair to the analog one has the analog pair
// alone, the USB pair silent, as with nothing plugged in there (XtFxImage::feedsUsb)
void XtFxUnit::frameIn(const int32_t* in, int i) {
    if (i >= kBlockFrames) return;
    XtFxChip& chip = *chip_;
    const int32_t l = in[2 * i] >> 8, r = in[2 * i + 1] >> 8;
    chip.setGram(kAnalogL, l);
    chip.setGram(kAnalogR, r);
    chip.setGram(kUsbL, feedsUsb_ ? l : 0);
    chip.setGram(kUsbR, feedsUsb_ ? r : 0);
}

// What the chip hands back for frame i, words 4 and 5 of its frame to the SH-4: the 24 bits at the top of an int32, as the
// SH-4's int32 frame reaches the chip (HARDWARE_FIRMWARE.md, "The input's way in": why words 4 and 5 are these)
void XtFxUnit::frameBack(int i) {
    if (i >= kBlockFrames) return;
    back_[2 * i] = (int32_t)((uint32_t)chip_->gram(kBackL) << 8);
    back_[2 * i + 1] = (int32_t)((uint32_t)chip_->gram(kBackR) << 8);
}
