// Each image's effect driver: see XtFxImage.h.  rom.4's addresses are rom.2's routines paired by the shape of their code
// (research/tools/codepairs.py) and by their callers; its IDs are the ones its readers were run reading, a block at a time.
#include "XtFxImage.h"

namespace {
using Row = XtFxImage::IdRow;
using enum XtFxImage::Block;

// [FW] A place in each image: rom.2's address, then rom.4's; 0 where that image has none to call
struct Place { uint32_t vSynth, vc2; };
using Side = uint32_t Place::*;

constexpr Place kSelf { 0x89ced1b4, 0x89980f24 }, kMessage { 0x89ced200, 0x89980f84 };
constexpr Place kClock { 0x89979bfc, 0x8995cb40 };
constexpr Place kDescLow { 0x897a167c, 0x8950f0b0 }, kDescHigh { 0x897a56d8, 0x895131a8 };
constexpr Place kMfxIds { 0x897a8218, 0x895164ec }, kChoIds { 0x897a82e4, 0x895165b8 };
constexpr Place kRevIds { 0x897a8310, 0x895165e4 };
constexpr Place kStaticInit { 0x894b4a60, 0x89368280 }, kStart { 0x894ac6a0, 0x8935f9a0 };
constexpr Place kStartTask { 0x894ac780, 0x8935fa80 }, kTakeMessage { 0x894ac970, 0x8935fc40 };
constexpr Place kClockPoll { 0x894acb10, 0x8935fde0 };
// rom.4 has none (v401): it sends its Matrix's MFX-PRM1…3 to the part mixer's levels (0x893a2e50), and no literal, word
// or branch reaches its MFX object's pair of rom.2's two (0x89361d50, 0x89361de0); its effect task's event 0x10 is the
// MFX sends by CC 91 and 93 (HARDWARE_FIRMWARE.md, "rom.4's Matrix MFX-PRMs")
constexpr Place kMatrixTake { 0x894ad4e0, 0 }, kMatrixApply { 0x894ad4f0, 0 };
// The EXT IN request and its event's routine (v400: rom.4's 0x8935b050 pairs rom.2's by what it does, External In Type to
// the routing's wanted byte 6, with no Source byte; its event 0x893620e0 is the effect task's event 0x40, as rom.2's)
constexpr Place kInputRequest { 0x894a7dd0, 0x8935b050 }, kInputEvent { 0x894aee80, 0x893620e0 };
// A control change (v402): rom.4's driver keeps CC 91 and 93 (0x893608a0, from its receiver 0x89360b90) and posts its
// event 0x10, whose routine (0x89360930) scales the MFX's Reverb and Chorus Sends by them.  rom.2's driver takes none
constexpr Place kControllerStore { 0, 0x893608a0 }, kControllerEvent { 0, 0x89360930 };
constexpr Place kSleep { 0x894b26a0, 0x89365e50 }, kPutWord { 0x894af400, 0x89362490 };
constexpr Place kGetWord { 0x894af510, 0x89362610 }, kPutCoefficient { 0x894af610, 0x893626d0 };
constexpr Place kPutOffset { 0x894af6a0, 0x893627f0 };
constexpr Place kRead { 0x8947d310, 0x89324350 }, kReadTyped { 0x8947d330, 0x89324370 };
// What returns 0 here: the two spins between bus cycles; the tone generator's lists, which the driver's start takes
// from; the end of the system's reader, an ID to the kernel; and the posts of an event to another task, which nothing here
// runs: the EXT IN request's 0x40 to the effect task (routeInput runs that event), and rom.4's 0x200 for a patch of
// Category 10 or 11
constexpr Place kSpinA { 0x894af3a0, 0x89362430 }, kSpinB { 0x894af3c0, 0x89362450 };
constexpr Place kListA { 0x894d9230, 0x893951b0 }, kListB { 0x894d90c0, 0x89395040 };
constexpr Place kListC { 0x894d9020, 0x89394fa0 };
constexpr Place kSystemEnd { 0x89301680, 0x89301610 }, kPostInput { 0x894aee70, 0x893620d0 };
constexpr Place kPostCategory { 0, 0x89391e50 };

constexpr Row kVSynthRows[] = {
    { 0x11d, 0x11d, BlockSwitches }, { 0x120, 0x122, BlockSwitches },
    { 0x51e, 0x51e, BlockAttenuation },
    { 0x3e, 0x3e, BlockSystem }, { 0x41, 0x41, BlockSystem }, { 0x76, 0x76, BlockSystem },
    { 0x65, 0x70, BlockSystem },
    { 0x78, 0x78, BlockInput }, { 0x7c, 0x7c, BlockInput },
    { 0x177, 0x17a, BlockMfx }, { 0x1fc, 0x1fe, BlockChorus }, { 0x230, 0x231, BlockReverb },
};

// The patch EQ, EQ 1 and EQ 2 (0x1af…0x1b4), is read with the switches: rom.4's own, in program A's two mid bands.
// External In Type (0x7a) is the EXT IN request's alone; USB External In Type (0x7f) the system reader's (v400)
constexpr Row kVc2Rows[] = {
    { 0x13a, 0x13a, BlockSwitches }, { 0x13d, 0x13f, BlockSwitches }, { 0x1af, 0x1b4, BlockSwitches },
    { 0x52b, 0x52b, BlockAttenuation },
    { 0x40, 0x40, BlockSystem }, { 0x43, 0x43, BlockSystem }, { 0x78, 0x78, BlockSystem },
    { 0x67, 0x68, BlockSystem }, { 0x6f, 0x72, BlockSystem }, { 0x7f, 0x7f, BlockSystem },
    { 0x7a, 0x7a, BlockInput },
    { 0x1b7, 0x1ba, BlockMfx }, { 0x23c, 0x23e, BlockChorus }, { 0x270, 0x271, BlockReverb },
};

// What an image is besides its places: its descriptors' IDs, the Reverb Types a patch can choose, which IDs each block's
// reader reads, its frame's gain into the chip and whether its USB pair hears the outside input
struct Facts {
    int firstHigh, ids, revTypes;
    std::span<const Row> rows;
    float frameGain;
    bool feedsUsb;
};

// An image's driver from its side of the places and its facts
constexpr XtFxImage imageOn(Side s, const Facts& f) {
    return {
        .self = kSelf.*s, .message = kMessage.*s, .clock = kClock.*s,
        .descLow = kDescLow.*s, .descHigh = kDescHigh.*s, .firstHigh = f.firstHigh, .ids = f.ids,
        .mfxIds = kMfxIds.*s, .choIds = kChoIds.*s, .revIds = kRevIds.*s,
        .mfxTypes = 42, .choTypes = 9, .revTypes = f.revTypes,
        .staticInit = kStaticInit.*s, .start = kStart.*s, .startTask = kStartTask.*s,
        .takeMessage = kTakeMessage.*s, .clockPoll = kClockPoll.*s,
        .matrixTake = kMatrixTake.*s, .matrixApply = kMatrixApply.*s,
        .inputRequest = kInputRequest.*s, .inputEvent = kInputEvent.*s,
        .controllerStore = kControllerStore.*s, .controllerEvent = kControllerEvent.*s,
        .sleep = kSleep.*s, .putWord = kPutWord.*s, .getWord = kGetWord.*s,
        .putCoefficient = kPutCoefficient.*s, .putOffset = kPutOffset.*s,
        .reads = { kRead.*s, kReadTyped.*s },
        .answered = { kSpinA.*s, kSpinB.*s, kListA.*s, kListB.*s, kListC.*s, kSystemEnd.*s, kPostInput.*s,
                      kPostCategory.*s },
        .idRows = f.rows,
        .frameGain = f.frameGain,
        .feedsUsb = f.feedsUsb,
    };
}

// [FW] each image's descriptors' IDs, and the Reverb Types a patch can choose (rom.4's descriptor: 0…14).  The frame's
// gains are XtFxImage.h's: rom.2's Out Gain for TG at its default, −24 dB, which the firmware's powf gives as exactly
// 1/16; rom.4's tones take theirs in the TVA's block (0x893a670c, as rom.2's 0x894ea380), so its frame goes in whole.
// The USB pair: XtFxImage.h's feedsUsb
constexpr float kVSynthFrameGain = 0.0625f, kVc2FrameGain = 1.0f;
constexpr XtFxImage kVSynth = imageOn(&Place::vSynth, { 0x55d, 0x8d0, 11, kVSynthRows, kVSynthFrameGain, true });
constexpr XtFxImage kVc2 = imageOn(&Place::vc2, { 0x56a, 0x9ac, 15, kVc2Rows, kVc2FrameGain, false });
} // namespace

const XtFxImage& xtVSynthFx() { return kVSynth; }
const XtFxImage& vc2Fx() { return kVc2; }
