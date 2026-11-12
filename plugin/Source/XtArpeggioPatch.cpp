// A patch's arpeggio as XtArpeggio takes it: see XtArpeggioPatch.h.
#include "XtArpeggioPatch.h"
#include "XtFiles.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <new>

namespace {
constexpr int kState = 0x44a, kNote = 0x44b, kStep1 = 0x44d, kStepIds = 2;   // a line's State, Note/CC and Step 1…32
constexpr int kPatternFirst = 0x449, kPatternLast = 0x48c;
constexpr int kCtrl = 0x80, kNoteBits = 0x7f, kLineShift = 16;
} // namespace

XtArpSettings xtArpSettingsOf(const std::vector<uint8_t>& store) {
    XtArpSettings s;
    s.on = xtPatchValue(store, kXtArpSwitchId) != 0;
    for (const XtArpSettingId& f : kXtArpSettingIds) s.*f.field = xtPatchValue(store, f.id);
    return s;
}

// The pattern as 0x8930ca30 lays it out: a line's State and Note/CC, then its 32 steps
XtArpPattern xtArpPatternOf(const std::vector<uint8_t>& store) {
    XtArpPattern p;
    for (int l = 0; l < XtArpPattern::kLines; ++l) {
        const bool ctrl = (xtPatchValue(store, kState, l) & 1) != 0;
        const int n = xtPatchValue(store, kNote, l) & kNoteBits;
        p.line[l] = (uint8_t)(ctrl ? kCtrl | std::min(n, kXtArpCtrlMost) : n);
        for (int s = 0; s < XtArpPattern::kSteps; ++s)
            p.cell[s][l] = (uint8_t)xtPatchValue(store, kStep1 + kStepIds * s, l);
    }
    return p;
}

XtPatchIds xtArpPatchIds(XtPatchIds p, const XtArpSettings& s, const XtArpPattern& pat) {
    p[kXtArpSwitchId] = s.on;
    for (const XtArpSettingId& f : kXtArpSettingIds) p[f.id] = s.*f.field;
    for (int l = 0; l < XtArpPattern::kLines; ++l) {
        const int by = l << kLineShift;
        p[kState | by] = (pat.line[l] & kCtrl) != 0;
        p[kNote | by] = pat.line[l] & kNoteBits;
        for (int st = 0; st < XtArpPattern::kSteps; ++st) p[(kStep1 + kStepIds * st) | by] = pat.cell[st][l];
    }
    return p;
}

bool xtArpByLine(int id) {
    return id >= kPatternFirst && id <= kPatternLast;
}

XtControls xtWithPatchArp(const std::vector<uint8_t>& store, XtControls c) {
    c.arp = xtArpSettingsOf(store);
    c.arpPattern = xtArpPatternOf(store);
    return c;
}

int xtToneWords(const XtControls& c) {
    return (int)((reinterpret_cast<const char*>(&c.arp) - reinterpret_cast<const char*>(&c)) / 4);
}

namespace {
struct alignas(XtControls) ControlsBytes {
    unsigned char b[sizeof(XtControls)];
};
using FieldBytes = std::array<bool, sizeof(XtControls)>;

// Which of the controls' bytes are a field's: made over 0x00s and over 0xffs, every field's default is written alike in
// both, and padding keeps what was there
FieldBytes fieldBytes() {
    ControlsBytes zeros, ones;
    std::memset(zeros.b, 0, sizeof zeros.b);
    std::memset(ones.b, 0xff, sizeof ones.b);
    new (zeros.b) XtControls;
    new (ones.b) XtControls;
    FieldBytes is{};
    for (size_t i = 0; i < is.size(); ++i) is[i] = zeros.b[i] == ones.b[i];
    return is;
}
} // namespace

bool xtToneWordMoved(const XtControls& a, const XtControls& b, size_t at) {
    static const FieldBytes field = fieldBytes();
    const auto* pa = reinterpret_cast<const unsigned char*>(&a);
    const auto* pb = reinterpret_cast<const unsigned char*>(&b);
    bool moved = false;
    for (size_t i = at; i < at + 4 && i < field.size(); ++i) moved = moved || (field[i] && pa[i] != pb[i]);
    return moved;
}
