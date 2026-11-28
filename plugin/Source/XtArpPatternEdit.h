#pragma once
// The XT's Pattern Edit (v468): what each of its edits writes into a patch's arpeggio pattern, and what its screen shows
// of a cell.  rom.2's screen (0x893f8200…0x893fb9xx) keeps a cursor, a line and a cell, in work IDs of the system store,
// and writes the patch through that store's setter (0x8937ced0), which clamps a value to its ID's range; its clears
// write rests.  HARDWARE_FIRMWARE.md, "The arpeggio's Pattern Edit".  The card on the PLAY page edits through these
// (UiArpPattern); xt_arp's edits section holds them to the firmware's own screen, setter and clears in the interpreter
// (XtArpPatternEditCheck.cpp).  No JUCE.
#include "XtArpeggio.h"
#include <string>

// Where the cursor is: the line (0x1403) and the step of its cell (0x1402 & 31)
struct XtArpCursor {
    int line = 0;
    int step = 0;
};

// The six buttons under the input area, 0…5 here (the handler's 10…15 [FW 0x893f9494]): 0, 32, 64, 96, 127, and Tie on a
// NOTE line or Clr on a CTRL line
inline constexpr int kXtArpButtons = 6;
// The cell a button writes at the cursor, by its line's State [FW 0x893f9400]
int xtArpButtonCell(int button, bool ctrl);
// A button's name on a line of that State
const char* xtArpButtonName(int button, bool ctrl);
// A button pressed at the cursor
void xtArpPress(XtArpPattern& p, XtArpCursor at, int button);

// 0x1401 set at the cursor: its cell, clamped to 0…128 [FW 0x8937a320]
void xtArpSetCell(XtArpPattern& p, XtArpCursor at, int value);
// A line's State (ID 0x44a, 0 NOTE or 1 CTRL) and Note/CC (0x44b, 0…127), clamped to the descriptors' ranges as the
// setter clamps them; the line's cells are left as they are
void xtArpSetState(XtArpPattern& p, int line, int state);
void xtArpSetNote(XtArpPattern& p, int line, int note);
bool xtArpIsCtrl(const XtArpPattern& p, int line);
int xtArpNoteOf(const XtArpPattern& p, int line);
// INC/DEC with SHIFT held or not: a cell's step, and a line's Note/CC's (12 on a NOTE line) [FW 0x893f8b10]
int xtArpCellStep(bool shift);
int xtArpNoteStep(bool ctrl, bool shift);

// LINE CLEAR and ALL CLEAR: Step 1…32 of the line, or of every line, to REST, each line's State and Note/CC kept
// [FW 0x89305360, 0x893052f0]
void xtArpClearLine(XtArpPattern& p, int line);
void xtArpClearAll(XtArpPattern& p);

// What an initialized patch has (0x89482540): every line NOTE C4, the descriptors' defaults, then line 1 at velocity 100
// on every step [FW 0x89482b1c].  Its pattern before any patch, the user's d-6 (v469).
XtArpPattern xtArpInitPattern();

// What the screen draws of a cell [FW 0x893f9fe0]: nothing for a rest; on a NOTE line a tie or a note; on a CTRL line a
// controller
enum class XtArpMark { none, note, tie, control };
XtArpMark xtArpMarkOf(const XtArpPattern& p, int line, int step);
// Value's text for the cell at the cursor [FW 0x893f9fe0]: on a NOTE line "  0" for a rest and "Tie" for a tie, on a CTRL
// line "***" for a rest, else the cell less 1, three characters wide
std::string xtArpValueText(const XtArpPattern& p, XtArpCursor at);
