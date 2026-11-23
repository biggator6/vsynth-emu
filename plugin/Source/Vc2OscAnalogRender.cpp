// The analog OSC's 19 renders (v337): the routines the installs write by the waveform's code and the sub oscillator's
// switch (0x893c2ad0's, 0x893c3e60's; Vc2OscAnalog.h), kind 3's in the voice's frame.  Each is rom.2's (XtAnalog.h),
// instruction for instruction, but for two things: its words lie four bytes up in rom.4's object from +0x94 (LITE's
// heads' and the band players' insert, vc2OscInRom4), and HQ-SAW and HQ-SQUARE reach HQ's tables through rom.4's own
// pointers (0x8993fbf8), whose tables are rom.2's byte for byte (Vc2OscAnalogPool::hq).  NOISE reaches +0x84 and the
// sub oscillator's TRIANGLE +0x108 by other instructions, the same words.  So XtAnalogOsc on the object's words
// (XtAnalogWords), the routine by its address, its words back.  Every address is rom.4's.  HARDWARE_FIRMWARE.md, "The
// VC-2's analog renders".  Audio thread.
#include "Vc2OscAnalog.h"
#include "Vc2OscPlayers.h"
#include "XtAnalogWords.h"
#include <algorithm>

std::optional<Vc2OscAnalog::Vc2AnalogRoutine> Vc2OscAnalog::routineAt(uint32_t at) {
    const auto* plain = std::ranges::find(kRenders, at);
    if (plain != kRenders.end()) return Vc2AnalogRoutine { (int)(plain - kRenders.begin()), false };
    const auto* sub = std::ranges::find(kSubRenders, at);
    if (sub != kSubRenders.end()) return Vc2AnalogRoutine { (int)(sub - kSubRenders.begin()), true };
    return std::nullopt;
}

std::optional<uint32_t> Vc2OscVoice::renderAnalog(uint32_t routine, float* out) {
    const std::optional<Vc2OscAnalog::Vc2AnalogRoutine> r = Vc2OscAnalog::routineAt(routine);
    if (!r) return std::nullopt;
    XtAnalogOsc osc;
    vc2OscWordsFrom(*this, &osc, XtAnalogWords::words(), vc2OscInRom4);
    XtAnalogWords::take(osc, r->code, r->sub, vc2OscTables().analog.hqTables());
    const uint32_t mask = osc.process(out, tail.sync);
    vc2OscWordsTo(*this, &osc, XtAnalogWords::words(), vc2OscInRom4);
    return mask;
}
