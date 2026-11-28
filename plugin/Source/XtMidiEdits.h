#pragma once
// rom.2's edits of the patch from a channel message (v462; HARDWARE_FIRMWARE.md, "rom.2's MIDI paths"): the OS's MIDI
// port, its RPN's and NRPN's numbers and Data Entry as the control change handler (0x89474a10) keeps them, and what its
// listener, the OS's part object (class 0x8982ffa4), writes into the patch through the edit object (0x8947a550,
// 0x89479e50, 0x8947a590) from its control change slot (0x89475ee0) and its RPN slot (0x89475f80).  A program change,
// the MIDI Tuning scale message and SysEx are not taken (the user's "Not now", 2026-11-27).  No JUCE.
#include <cstdint>

// One value the patch takes: the firmware's parameter ID and its value as the patch holds it
struct XtStoredEdit {
    int id = 0;
    int value = 0;
};

// What one message edits, in the order the firmware writes them: two at most (RPN 0 writes both Bend Ranges)
struct XtStoredEdits {
    XtStoredEdit at[2];
    int count = 0;
    void add(int id, int value) { at[count++] = { id, value }; }
};

// The IDs these edits write: Patch Coarse and Fine Tune, Mono/Poly, Portamento Switch and Time, Bend Range Up and Down
namespace xtedit {
constexpr int CoarseTune = 0x110, FineTune = 0x111, MonoPoly = 0x113, PortaSwitch = 0x115, PortaTime = 0x118;
constexpr int BendUp = 0x11e, BendDown = 0x11f;
}

// The port object's words +4, +6 and +8, and a control change on its channel through the handler and the part object
struct XtMidiPort {
    static constexpr uint16_t kNone = 0x7f7f;         // no number (CC 121 at 0 puts both back)
    static constexpr uint16_t kUnselected = 0x8000;   // the other kind was chosen last
    uint16_t rpn = kNone, nrpn = kNone;
    uint16_t data = 0;                                // Data Entry: its MSB << 7 | its LSB
    XtStoredEdits controlChange(int cc, int value);
};
