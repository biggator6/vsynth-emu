// The XT's Pattern Edit: see XtArpPatternEdit.h.  Each value is the firmware's at the address beside it.
#include "XtArpPatternEdit.h"
#include <algorithm>
#include <cstdio>

namespace {
constexpr int kRest = 0, kTie = 1;                    // a cell's REST and TIE, a CTRL line's value 0 [FW 0x89302700]
constexpr int kCellMost = 128;                        // 0x1401's range, 0…128 [FW 0x8937a320]
constexpr int kNoteMost = 127;                        // ID 0x44b's descriptor [FW 0x897a167c]
constexpr uint8_t kCtrlBit = 0x80, kNoteBits = 0x7f;  // a line's State over its Note/CC
constexpr int kInitNote = 60;                         // ID 0x44b's default, C4 [FW 0x897a167c]
constexpr uint8_t kInitCell = 101;                    // velocity 100 on line 1's every step [FW 0x89482b32]
constexpr int kCellShift = 10, kNoteShift = 12, kCtrlShift = 10;   // INC/DEC with SHIFT [FW 0x893f8b10]

// What each button writes: on a NOTE line, and on a CTRL line [FW 0x893f9400, by the table at 0x893f9494]
struct Button {
    int note, ctrl;
    const char *noteName, *ctrlName;
};
constexpr Button kPatternButtons[kXtArpButtons] = {
    { kRest, kTie, "0", "0" },        // 0x893f99f0 (a rest), 0x893f94b8 (the value 0)
    { 33, 33, "32", "32" },           // 0x893f9930
    { 65, 65, "64", "64" },           // 0x893f9960
    { 97, 97, "96", "96" },           // 0x893f998c
    { 128, 128, "127", "127" },       // 0x893f99bc
    { kTie, kRest, "Tie", "Clr" },    // 0x893f9a1c (a tie), 0x893f99f0 (a rest)
};

// A cell's kind, which the screen draws by with its line's State: a rest, a 1 (a tie, or a controller's 0), a value
enum CellKind { kRestKind, kOneKind, kValueKind, kCellKinds };
CellKind cellKind(int cell) {
    return (CellKind)std::min(cell, (int)kValueKind);
}

// 0x893f9fe0, by State then kind: its glyphs (0x97a a rest, 0x977 a tie, 0x976 a note, 0x978 a controller) and Value's
// text at (0x37, 0xad), "  0" (0x89778740), "Tie" (0x897786ec), "***" (0x8977873c) or, with none here, the number
struct CellShown {
    XtArpMark mark;
    const char* text;
};
constexpr CellShown kCellShown[2][kCellKinds] = {
    { { XtArpMark::none, "  0" }, { XtArpMark::tie, "Tie" }, { XtArpMark::note, nullptr } },                // NOTE
    { { XtArpMark::none, "***" }, { XtArpMark::control, nullptr }, { XtArpMark::control, nullptr } },       // CTRL
};

bool onThePattern(int line, int step) {
    return line >= 0 && line < XtArpPattern::kLines && step >= 0 && step < XtArpPattern::kSteps;
}

const CellShown& cellShown(const XtArpPattern& p, XtArpCursor at) {
    return kCellShown[xtArpIsCtrl(p, at.line) ? 1 : 0][cellKind(p.cell[at.step][at.line])];
}
} // namespace

int xtArpButtonCell(int button, bool ctrl) {
    const Button& b = kPatternButtons[std::clamp(button, 0, kXtArpButtons - 1)];
    return ctrl ? b.ctrl : b.note;
}

const char* xtArpButtonName(int button, bool ctrl) {
    const Button& b = kPatternButtons[std::clamp(button, 0, kXtArpButtons - 1)];
    return ctrl ? b.ctrlName : b.noteName;
}

void xtArpPress(XtArpPattern& p, XtArpCursor at, int button) {
    if (onThePattern(at.line, at.step)) xtArpSetCell(p, at, xtArpButtonCell(button, xtArpIsCtrl(p, at.line)));
}

// 0x893787f0's case 0x1401 (0x89379a44): the cell at the cursor's line and step, after the setter's clamp (0x8937ced0)
void xtArpSetCell(XtArpPattern& p, XtArpCursor at, int value) {
    if (onThePattern(at.line, at.step)) p.cell[at.step][at.line] = (uint8_t)std::clamp(value, kRest, kCellMost);
}

void xtArpSetState(XtArpPattern& p, int line, int state) {
    if (!onThePattern(line, 0)) return;
    const uint8_t note = p.line[line] & kNoteBits;
    p.line[line] = std::clamp(state, 0, 1) != 0 ? (uint8_t)(kCtrlBit | note) : note;
}

void xtArpSetNote(XtArpPattern& p, int line, int note) {
    if (!onThePattern(line, 0)) return;
    p.line[line] = (uint8_t)((p.line[line] & kCtrlBit) | std::clamp(note, 0, kNoteMost));
}

bool xtArpIsCtrl(const XtArpPattern& p, int line) {
    return onThePattern(line, 0) && (p.line[line] & kCtrlBit) != 0;
}

int xtArpNoteOf(const XtArpPattern& p, int line) {
    return onThePattern(line, 0) ? p.line[line] & kNoteBits : 0;
}

int xtArpCellStep(bool shift) {
    return shift ? kCellShift : 1;
}

// A NOTE line's Note/CC by an octave with SHIFT (keys 0x13 and 0x15); a CTRL line's by the screen's own step, not read
// past it, taken as a cell's
int xtArpNoteStep(bool ctrl, bool shift) {
    if (!shift) return 1;
    return ctrl ? kCtrlShift : kNoteShift;
}

// 0x89305360 (LINE CLEAR) and 0x893052f0 (ALL CLEAR): 0 to IDs 0x44d, 0x44f … 0x48b of the line (0x89479e50)
void xtArpClearLine(XtArpPattern& p, int line) {
    if (!onThePattern(line, 0)) return;
    for (auto& step : p.cell) step[line] = kRest;
}

void xtArpClearAll(XtArpPattern& p) {
    for (int line = 0; line < XtArpPattern::kLines; ++line) xtArpClearLine(p, line);
}

// 0x89482540: the pattern's 16 lines to the descriptors' defaults (0x89482834 on 0x44a…, NOTE, C4, every cell 0), then
// 101 to Step 1…32 of line 1 (0x89482b1c…0x89482b3c)
XtArpPattern xtArpInitPattern() {
    XtArpPattern p;
    std::fill(std::begin(p.line), std::end(p.line), (uint8_t)kInitNote);
    for (auto& step : p.cell) step[0] = kInitCell;
    return p;
}

XtArpMark xtArpMarkOf(const XtArpPattern& p, int line, int step) {
    return onThePattern(line, step) ? cellShown(p, { line, step }).mark : XtArpMark::none;
}

// Off the input area, "---" (0x89778744); a value, the cell less 1 in three places (0x89325cb0)
std::string xtArpValueText(const XtArpPattern& p, XtArpCursor at) {
    if (!onThePattern(at.line, at.step)) return "---";
    if (const char* text = cellShown(p, at).text) return text;
    char number[8];
    std::snprintf(number, sizeof number, "%3d", p.cell[at.step][at.line] - 1);
    return number;
}
