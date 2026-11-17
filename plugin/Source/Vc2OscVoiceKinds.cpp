// The OSC voice's kinds' rows: see Vc2OscVoiceKinds.h (v230; the note-on's from v232).  Every address is rom.4's, each
// field from its routine's jump table (HARDWARE_FIRMWARE.md, "The VC-2's OSC voice").
#include "Vc2OscVoiceKinds.h"
#include <array>

namespace {
using Records = Vc2OscRecords;
using Render = Vc2OscRender;
using Pitch = Vc2OscPitch;
using NoteOn = Vc2OscNoteOn;
using Install = Vc2OscInstall;
constexpr uint8_t kR0 = kVc2OscRamp0, kR1 = kVc2OscRamp1, kR2 = kVc2OscRamp2;

// [FW] kinds 0…7, then a kind past 7.  Kinds 1 and 2's installs (0x893af7a0, 0x893af870) and kind 2's note-on
// (0x893af420) are ported from v238 (Vc2OscPlayers.cpp), kind 1's note-on (0x893c7c30) from v240 (Vc2OscLite.cpp),
// kind 3's (0x893c22e0) from v242 (Vc2OscAnalog.cpp)
constexpr std::array<Vc2OscKindRow, kVc2OscKinds + 1> kRows { {
    { .clearsFlag = true, .renders = Render::zero, .noteOn = NoteOn::one },
    { .installs = Install::lite, .waits = true, .ticks = kR0 | kR2, .update = 0x893c8280,
      .records = Records::fromBlock, .ramp = true, .settings = 0x893c8390, .pitch = Pitch::tuned,
      .noteOn = NoteOn::lite },
    { .installs = Install::vari, .waits = true, .ticks = kR0 | kR1 | kR2, .update = 0x893c88b0,
      .records = Records::fromBlock, .settings = 0x893b0280, .pitch = Pitch::tuned, .noteOn = NoteOn::vari },
    { .installs = Install::routine, .install = 0x893c2ad0, .ticks = kR0, .update = 0x893c2ce0, .ramp = true,
      .settings = 0x893c3460, .renders = Render::byAnalog, .pitch = Pitch::bounded, .noteOn = NoteOn::analog },
    { .render = 0x893c62e0, .installed = 4, .ticks = kR0, .update = 0x893c6260, .flagByWave = true,
      .noteOn = NoteOn::extIn },
    { .installed = 5, .ticks = kR0, .update = 0x893afff0, .ramp = true, .carrier = true, .renders = Render::carrier,
      .pitch = Pitch::bounded, .noteOn = NoteOn::carrier },
    { .installed = 6, .updateFirst = 0x893c6260, .ticks = kR0 | kR1, .update = 0x893b0150, .ramp = true,
      .renders = Render::kind6, .pitch = Pitch::bounded, .noteOn = NoteOn::kind6 },
    { .render = 0x893c61e0, .installed = 4, .ticks = kR0, .update = 0x893c6260, .flagByWave = true,
      .noteOn = NoteOn::extIn },
    { .clearsFlag = true, .records = Records::atZero },
} };
} // namespace

const Vc2OscKindRow& vc2OscKindRow(uint32_t kind) { return kRows[kind < (uint32_t)kVc2OscKinds ? kind : kVc2OscKinds]; }
