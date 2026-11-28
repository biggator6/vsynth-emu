// The arpeggio's pattern on the PLAY page (UiArpPattern.h; UI step 4; edited by the XT's rules from v468)
#include "UiArpPattern.h"
#include "PluginArp.h"
#include "UiLook.h"
#include "UiParts.h"

namespace {

constexpr int kStateW = 34, kNoteW = 34, kNamesGap = 6;   // a line's State and Note/CC at the left of its cells
constexpr int kBeat = 4;                                  // the steps between the grid's faint lines
constexpr int kDragPx = 6;                                // a drag's pixels a step of its value
constexpr float kFullVelocity = 127.0f;
constexpr float kPastEnd = 0.35f;                         // a cell past the End Point, this much as strong

// A line's Note/CC as its State shows it: a note's name, or the controller's number
juce::String lineNumberText(const XtArpPattern& p, int line) {
    const int n = xtArpNoteOf(p, line);
    return xtArpIsCtrl(p, line) ? juce::String(n) : Ui::keyName(n);
}

// A cell's strength, 0.3 for the least velocity or value to 1 for the most
float strength(uint8_t cell) {
    return 0.3f + 0.7f * (float)juce::jmax(0, cell - 1) / kFullVelocity;
}

// Where the i-th of the grid's n equal rows or columns starts along a span
int gridLineAt(int start, int span, int i, int n) {
    return start + juce::roundToInt((float)i * (float)span / (float)n);
}

}

ArpPatternView::ArpPatternView(VSynthEmuProcessor& p)
    : ArpPatternView(p, { [&p] { return p.arpPattern(); }, "arpPattern", "arpEndPoint",
                          [&p](const XtArpPattern& edited) { p.setArpPattern(edited); } }) {}

ArpPatternView::ArpPatternView(VSynthEmuProcessor& p, Source source)
    : source_(std::move(source)), end_(p.apvts, { source_.endPointId }, [this] { repaint(); }) {
    pattern_.referTo(p.apvts.state.getPropertyAsValue(source_.property, nullptr));
    pattern_.addListener(this);
}

ArpPatternView::~ArpPatternView() {
    pattern_.removeListener(this);
}

void ArpPatternView::valueChanged(juce::Value&) {
    repaint();
    if (onChange) onChange();
}

juce::Rectangle<int> ArpPatternView::grid() const {
    return getLocalBounds().reduced(6).withTrimmedLeft(kStateW + kNoteW + kNamesGap);
}

juce::Rectangle<int> ArpPatternView::cellBounds(int line, int step) const {
    const auto g = grid();
    const int x0 = gridLineAt(g.getX(), g.getWidth(), step, XtArpPattern::kSteps);
    const int x1 = gridLineAt(g.getX(), g.getWidth(), step + 1, XtArpPattern::kSteps);
    const int y0 = gridLineAt(g.getY(), g.getHeight(), line, XtArpPattern::kLines);
    const int y1 = gridLineAt(g.getY(), g.getHeight(), line + 1, XtArpPattern::kLines);
    return { x0, y0, x1 - x0, y1 - y0 };
}

juce::Rectangle<int> ArpPatternView::stateBounds(int line) const {
    const auto cell = cellBounds(line, 0);
    return { getLocalBounds().reduced(6).getX(), cell.getY(), kStateW, cell.getHeight() };
}

juce::Rectangle<int> ArpPatternView::noteBounds(int line) const {
    return stateBounds(line).translated(kStateW, 0).withWidth(kNoteW);
}

void ArpPatternView::setCursor(XtArpCursor at) {
    cursor_ = { juce::jlimit(0, XtArpPattern::kLines - 1, at.line),
                juce::jlimit(0, XtArpPattern::kSteps - 1, at.step) };
    repaint();
    if (onChange) onChange();
}

// The property's listeners hear of it later (a Value's are told asynchronously), so what follows the pattern here is
// told now
void ArpPatternView::edit(const std::function<void(XtArpPattern&)>& change) {
    if (!editable()) return;
    XtArpPattern p = source_.pattern();
    change(p);
    source_.set(p);
    repaint();
    if (onChange) onChange();
}

// A cell's value at the cursor, or a line's Note/CC, moved `by` steps of INC/DEC's (XtArpPatternEdit.h)
void ArpPatternView::nudge(Press::On on, int line, int by, bool shift) {
    const XtArpCursor at = cursor_;
    if (on == Press::On::cell)
        edit([&](XtArpPattern& p) { xtArpSetCell(p, at, p.cell[at.step][at.line] + by * xtArpCellStep(shift)); });
    if (on != Press::On::note) return;
    edit([&](XtArpPattern& p) {
        xtArpSetNote(p, line, xtArpNoteOf(p, line) + by * xtArpNoteStep(xtArpIsCtrl(p, line), shift));
    });
}

// A press: on a State, that line's State turned; on a Note/CC, a drag of it to come; on a cell, the cursor there and a
// drag of its value to come
void ArpPatternView::mouseDown(const juce::MouseEvent& e) {
    press_ = {};
    if (!editable()) return;
    const juce::Point<int> at = e.getPosition();
    for (int line = 0; line < XtArpPattern::kLines; ++line) {
        if (stateBounds(line).contains(at)) {
            edit([line](XtArpPattern& p) { xtArpSetState(p, line, xtArpIsCtrl(p, line) ? 0 : 1); });
            return;
        }
        if (noteBounds(line).contains(at)) {
            press_ = { Press::On::note, line, at.y, 0 };
            return;
        }
        for (int step = 0; step < XtArpPattern::kSteps; ++step)
            if (cellBounds(line, step).contains(at)) {
                setCursor({ line, step });
                press_ = { Press::On::cell, line, at.y, 0 };
                return;
            }
    }
}

void ArpPatternView::mouseDrag(const juce::MouseEvent& e) {
    if (press_.on == Press::On::nothing) return;
    const int steps = (press_.startY - e.getPosition().y) / kDragPx;
    if (steps == press_.moved) return;
    nudge(press_.on, press_.line, steps - press_.moved, e.mods.isShiftDown());
    press_.moved = steps;
}

// The wheel over a Note/CC moves it; over a cell, the cursor goes there and its value moves
void ArpPatternView::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) {
    if (!editable() || w.deltaY == 0.0f) return;
    const int by = w.deltaY > 0.0f ? 1 : -1;
    const juce::Point<int> at = e.getPosition();
    for (int line = 0; line < XtArpPattern::kLines; ++line) {
        if (noteBounds(line).contains(at)) return nudge(Press::On::note, line, by, e.mods.isShiftDown());
        for (int step = 0; step < XtArpPattern::kSteps; ++step)
            if (cellBounds(line, step).contains(at)) {
                setCursor({ line, step });
                return nudge(Press::On::cell, line, by, e.mods.isShiftDown());
            }
    }
}

// Each line's State and Note/CC, a CTRL line's in `mod`
void ArpPatternView::paintLines(juce::Graphics& g, const XtArpPattern& pattern) const {
    for (int line = 0; line < XtArpPattern::kLines; ++line) {
        const bool ctrl = xtArpIsCtrl(pattern, line);
        const Ui::TextStyle state { Look::mono(9.0f), ctrl ? Look::mod : Look::label };
        Ui::drawText(g, ctrl ? "CTRL" : "NOTE", stateBounds(line), state);
        Ui::drawText(g, lineNumberText(pattern, line), noteBounds(line), { Look::mono(9.0f), Look::text });
    }
}

// The cells as the XT marks them, faint past the End Point, a line each beat; the cursor's ringed
void ArpPatternView::paintCells(juce::Graphics& g, const XtArpPattern& pattern) const {
    const int end = juce::roundToInt(end_.value(0));
    const auto field = grid();
    for (int step = kBeat; step < XtArpPattern::kSteps; step += kBeat)
        Ui::fill(g, { cellBounds(0, step).getX(), field.getY(), 1, field.getHeight() }, Look::divider);
    for (int line = 0; line < XtArpPattern::kLines; ++line)
        for (int step = 0; step < XtArpPattern::kSteps; ++step) {
            const XtArpMark mark = xtArpMarkOf(pattern, line, step);
            if (mark == XtArpMark::none) continue;
            const auto at = cellBounds(line, step).toFloat();
            const float alpha = (mark == XtArpMark::tie ? 0.3f : strength(pattern.cell[step][line]))
                                * (step > end ? kPastEnd : 1.0f);
            if (mark == XtArpMark::control) {
                const float d = juce::jmin(at.getWidth(), at.getHeight()) - 4.0f;
                g.setColour(Look::mod.withAlpha(alpha));
                g.drawEllipse(juce::Rectangle<float>(d, d).withCentre(at.getCentre()), 1.5f);
                continue;
            }
            const float h = at.getHeight();
            const auto shape = mark == XtArpMark::tie ? at.withTrimmedTop(h * 0.35f).withTrimmedBottom(h * 0.35f)
                                                      : at.reduced(1.0f, 1.0f);
            Ui::fillRounded(g, shape, Look::accent.withAlpha(alpha), 2.0f);
        }
    if (!editable()) return;
    g.setColour(Look::text);
    g.drawRect(cellBounds(cursor_.line, cursor_.step), 1);
}

void ArpPatternView::paint(juce::Graphics& g) {
    const XtArpPattern pattern = source_.pattern();
    Ui::fillRounded(g, getLocalBounds().toFloat(), Look::field, 6.0f);
    paintLines(g, pattern);
    paintCells(g, pattern);
    if (!xtArpPatternSilent(pattern)) return;
    Ui::drawText(g, "This pattern has no step to play: the arpeggio is silent", grid(),
                 { Look::sans(12.0f), Look::muted, juce::Justification::centred });
}
