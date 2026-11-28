// xt_arp's edits section (XT_ARP_MAIN; v468): the XT's Pattern Edit run in the interpreter on a patch's store against
// XtArpPatternEdit.  Part 0's stores are the patch (+4 and +8 its reads', +0x10 the write object's, 0x8947b5b0); the
// screen's cursor goes in through the system store's setter (0x8937ced0, which clamps), and so does each edit: 0x1401 at
// the cursor, a line's State and Note/CC; the six buttons are the screen's own handler (0x893f9400); LINE CLEAR and ALL
// CLEAR the Pattern Edit object's (0x89304010, 0x89304000).  After each, the store's pattern (read by xtArpPatternOf, which
// xt_files holds to the firmware's getters) against the port's; now and then the arpeggio's own read of it (0x8930ca30,
// a CTRL line's controller made 95 at most) against xtArpLaidOut, and the screen's redraw (0x893f9fe0): Value's text and
// every cell's glyph against the port's.  And the pattern before any patch, xtArpInitPattern, against the
// firmware's default patch (0x8947e230, then 0x8947dae0 on no bytes).  HARDWARE_FIRMWARE.md, "The arpeggio's Pattern Edit".
#ifdef XT_ARP_MAIN
#include "XtArpPatternEdit.h"
#include "XtArpeggioCheck.h"
#include "XtArpeggioPatch.h"
#include "XtFirmware.h"
#include "XtSwitches.h"
#include <cstdio>
#include <functional>
#include <random>

namespace {
constexpr uint32_t kParts = 0x897a0fc8;                         // part 0's object, its flag and stores [FW 0x8947b5b0]
constexpr uint32_t kWork = 0x89968728, kSet = 0x8937ced0;       // the system store and its setter
constexpr int kCellId = 0x1401, kCellAtId = 0x1402, kLineId = 0x1403, kStateId = 0x44a, kNoteId = 0x44b;
constexpr uint32_t kPress = 0x893f9400, kRedraw = 0x893f9fe0, kLineClear = 0x89304010, kAllClear = 0x89304000;
constexpr uint32_t kFocus = 0x89374e00, kInputArea = 0x20;      // the cursor's field
constexpr uint32_t kText = 0x89325310, kGlyph = 0x89324660;     // what the redraw draws through
constexpr uint32_t kLayOut = 0x8930ca30, kLaidOut = 0x89880f83;  // the arpeggio's read of part 0's pattern, and its buffer
constexpr uint32_t kScreen = xtfw::kSamp + 0x4000;              // the screen object the handlers are given (unread)
constexpr int kFirstButton = 10;                                // the handler's 10…15 [FW 0x893f9494]
// Answered with nothing: the OS's semaphores; the screen's lock and drawing; Rec's tables, ALL CLEAR's last call
// (0x893057f0), after its writes
constexpr uint32_t kQuiet[] = { 0x89301920, 0x89301930, 0x893c37d0, 0x8932cdb0, 0x89323670, 0x89323910, 0x893241a0,
                                0x89323660, 0x893240d0, 0x89323d00, 0x89323800, 0x89323f30, 0x89323970, 0x8932ce20,
                                0x893fabd0, 0x893057f0 };
constexpr int kValueX = 0x37, kValueY = 0xad;                   // Value's place
constexpr int kGridX = 0x66, kGridY = 0x9b, kCellW = 6, kLineH = 7;   // a cell's glyph, at x + 6·step, y − 7·line
constexpr int kCursorGlyph = 0x979;                             // drawn over the cursor's cell after its own
constexpr int kEditsACase = 60, kSaid = 20;

// The glyph the redraw drew for each mark [FW 0x893f9fe0]
XtArpMark markOf(int glyph) {
    constexpr int kGlyphs[] = { 0x97a, 0x976, 0x977, 0x978 };   // none, note, tie, control
    for (int m = 0; m < 4; ++m)
        if (kGlyphs[m] == glyph) return (XtArpMark)m;
    return (XtArpMark)-1;
}

bool inGrid(int line, int step) {
    return step >= 0 && step < XtArpPattern::kSteps && line >= 0 && line < XtArpPattern::kLines;
}

class FwEdit {
public:
    explicit FwEdit(const std::string& rom) : m(rom) {
        if (!m.ok) return;
        const uint32_t part = m.cpu.rd32(kParts);
        m.cpu.wr8(part, 1);
        for (const uint32_t at : { 4u, 8u, 0x10u }) m.cpu.wr32(part + at, xtfw::kSamp);
        for (const uint32_t a : kQuiet) m.cpu.hook(a, [](Sh4& c) { c.ret(0); });
        m.cpu.hook(kFocus, [](Sh4& c) { c.ret(kInputArea); });
        m.cpu.hook(kText, [this](Sh4& c) {
            char t[16] = {};
            c.read(c.r[7], t, sizeof t - 1);
            if ((int)c.r[5] == kValueX && (int)c.r[6] == kValueY) value = t;
            c.ret(0);
        });
        m.cpu.hook(kGlyph, [this](Sh4& c) {
            const int x = (int16_t)c.r[5], y = (int16_t)c.r[6];
            const int step = (x - kGridX) / kCellW, line = (kGridY - y) / kLineH;
            if ((int)c.r[7] != kCursorGlyph && inGrid(line, step)) glyph[line][step] = (int)c.r[7];
            c.ret(0);
        });
    }
    bool ok() const { return m.ok && failed.empty(); }
    void load(const std::vector<uint8_t>& s) {
        bytes = s.size();
        m.cpu.write(xtfw::kSamp, s.data(), s.size());
    }
    XtArpPattern pattern() {
        std::vector<uint8_t> s(bytes);
        m.cpu.read(xtfw::kSamp, s.data(), s.size());
        return xtArpPatternOf(s);
    }
    // 0x8937ced0(store, id, part, index, value): the index (a pattern ID's line) in r7, the value on the stack
    void set(int id, int line, int value) {
        m.cpu.wr32(xtfw::kStack, (uint32_t)value);
        call(kSet, { kWork, (uint32_t)id, 0xffffffffu, (uint32_t)line });
    }
    void call(uint32_t addr, std::initializer_list<uint32_t> args) {
        if (failed.empty() && !m.cpu.call(addr, args, xtfw::kStack)) failed = m.cpu.error();
    }
    void redraw() {
        value = "?";
        for (auto& line : glyph) std::fill(std::begin(line), std::end(line), 0);
        call(kRedraw, { kScreen });
    }
    // The pattern as the arpeggio takes it from part 0's store: its lines, then its cells by step then line
    XtArpPattern laidOut() {
        XtArpPattern p;
        call(kLayOut, { 0, 0 });
        m.cpu.read(kLaidOut, p.line, sizeof p.line);
        m.cpu.read(kLaidOut + sizeof p.line, p.cell, sizeof p.cell);
        return p;
    }
    std::string value, failed;
    int glyph[XtArpPattern::kLines][XtArpPattern::kSteps] = {};

private:
    xtfw::Machine m;
    size_t bytes = 0;
};

struct EditCase {
    FwEdit& fw;
    XtArpTally& t;
    std::mt19937& r;
    bool first;
    XtArpPattern port;
    XtArpCursor at;
    int pick(int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(r); }
    void differs(const std::string& what) {
        if (++t.different <= kSaid && first) std::printf("edits case %ld: %s\n", t.cases, what.c_str());
    }
    void same(const char* what, int fwValue, int portValue) {
        ++t.values;
        if (fwValue == portValue) return;
        differs(std::string(what) + ": firmware " + std::to_string(fwValue) + ", port " + std::to_string(portValue));
    }
    void compare() {
        const XtArpPattern got = fw.pattern();
        for (int l = 0; l < XtArpPattern::kLines; ++l) {
            same("line", got.line[l], port.line[l]);
            for (int s = 0; s < XtArpPattern::kSteps; ++s) same("cell", got.cell[s][l], port.cell[s][l]);
        }
    }
    // What the redraw shows, and the pattern as the arpeggio lays it out (0x8930ca30) against xtArpLaidOut
    void compareShown() {
        const XtArpPattern laid = fw.laidOut(), portLaid = xtArpLaidOut(port);
        for (int l = 0; l < XtArpPattern::kLines; ++l) {
            same("laid-out line", laid.line[l], portLaid.line[l]);
            for (int s = 0; s < XtArpPattern::kSteps; ++s) same("laid-out cell", laid.cell[s][l], portLaid.cell[s][l]);
        }
        fw.redraw();
        ++t.values;
        const std::string want = xtArpValueText(port, at);
        if (fw.value != want) differs("Value: firmware '" + fw.value + "', port '" + want + "'");
        for (int l = 0; l < XtArpPattern::kLines; ++l)
            for (int s = 0; s < XtArpPattern::kSteps; ++s)
                same("mark", (int)markOf(fw.glyph[l][s]), (int)xtArpMarkOf(port, l, s));
    }

    // The edits, each on both sides, with how often each is drawn in a hundred: the cursor moved, a value at it, a
    // button, a line's State or Note/CC (each beyond its range too), LINE CLEAR, ALL CLEAR
    void cursor(int line) {
        at = { line, pick(0, XtArpPattern::kSteps - 1) };
        fw.set(kLineId, -1, at.line);
        fw.set(kCellAtId, -1, at.line * XtArpPattern::kSteps + at.step);
    }
    void cellValue(int) {
        const int v = pick(-20, 150);
        fw.set(kCellId, -1, v);
        xtArpSetCell(port, at, v);
    }
    void button(int) {
        const int b = pick(0, kXtArpButtons - 1);
        fw.call(kPress, { kScreen, (uint32_t)(kFirstButton + b) });
        xtArpPress(port, at, b);
    }
    void state(int line) {
        const int v = pick(-2, 3);
        fw.set(kStateId, line, v);
        xtArpSetState(port, line, v);
    }
    void note(int line) {
        const int v = pick(-20, 150);
        fw.set(kNoteId, line, v);
        xtArpSetNote(port, line, v);
    }
    void clearLine(int line) {
        fw.call(kLineClear, { 0, (uint32_t)line });
        xtArpClearLine(port, line);
    }
    void clearAll(int) {
        fw.call(kAllClear, {});
        xtArpClearAll(port);
    }
    struct Kind {
        int often;
        void (EditCase::*run)(int line);
    };
    static constexpr Kind kKinds[] = { { 25, &EditCase::cursor }, { 15, &EditCase::cellValue },
                                       { 32, &EditCase::button }, { 10, &EditCase::state },
                                       { 11, &EditCase::note },   { 6, &EditCase::clearLine },
                                       { 1, &EditCase::clearAll } };
    void edit() {
        int draw = pick(0, 99);
        const int line = pick(0, XtArpPattern::kLines - 1);
        for (const Kind& k : kKinds) {
            if (draw < k.often) {
                (this->*k.run)(line);
                break;
            }
            draw -= k.often;
        }
        ++t.steps;
    }
};

std::vector<uint8_t> defaultStore() {
    std::vector<uint8_t> store;
    const uint8_t none = 0;
    return xtFwPatchStore(&none, 0, store) ? store : std::vector<uint8_t> {};
}
} // namespace

XtArpTally xtArpPatternEditCheck(const std::string& rom, long cases, unsigned seed, bool first) {
    XtArpTally t;
    FwEdit fw(rom);
    const std::vector<uint8_t> store = defaultStore();
    if (!fw.ok() || store.empty()) {
        t.failed = true;
        return t;
    }
    std::mt19937 r(seed + 468);   // its own draws, so no other section's cases move
    const bool stop = xtSwitchIs("XT_ARP_STOP", "1");
    EditCase c { fw, t, r, first, xtArpInitPattern(), {} };   // the pattern before any patch (d-6) as the default's
    fw.load(store);
    c.compare();
    for (; t.cases < cases && fw.ok() && !(stop && t.different); ++t.cases) {
        fw.load(store);
        c.port = xtArpPatternOf(store);
        c.at = {};
        fw.set(kLineId, -1, 0);
        fw.set(kCellAtId, -1, 0);
        for (int e = 0; e < kEditsACase && fw.ok(); ++e) {
            c.edit();
            c.compare();
            if (c.pick(0, 3) == 0) c.compareShown();
        }
    }
    if (!fw.ok()) {
        std::printf("edits: a routine failed: %s\n", fw.failed.c_str());
        t.failed = true;
    }
    return t;
}
#endif
