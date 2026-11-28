// VSynthEmuEditorTest's check of the arpeggio pattern's card (EditorTest.h; v468): the pattern edited on the PLAY page as
// the XT's Pattern Edit edits it (XtArpPatternEdit, which xt_arp holds to the firmware's screen), each press, drag and
// wheel through the card into the session, against the same edit made on the pattern by XtArpPatternEdit; Value and the
// buttons' names following the cursor; the clears and INIT; and the edited pattern brought back by a saved session and
// read into the controls the synth plays.
#include "EditorTest.h"
#include "../Source/PluginArp.h"
#include "../Source/UiArpPatternEdit.h"
#include "../Source/UiPlayPage.h"

namespace {

constexpr int kDragPx = 6;   // UiArpPattern.cpp's pixels a step

void pressLift(juce::Component& c, juce::Point<int> at) {
    LayoutCheck::pressAt(c, at.toFloat());
    c.mouseUp(LayoutCheck::mouseAt(c, at.toFloat(), at.toFloat()));
}

void wheelBy(juce::Component& c, juce::Point<int> at, float deltaY) {
    juce::MouseWheelDetails w;
    w.deltaX = 0.0f;
    w.deltaY = deltaY;
    w.isReversed = false;
    w.isSmooth = false;
    w.isInertial = false;
    c.mouseWheelMove(LayoutCheck::mouseAt(c, at.toFloat(), at.toFloat()), w);
}

struct PatternCard {
    ArpPatternEditor& ed;
    VSynthEmuProcessor& p;
    TestChecks& check;
    XtArpPattern want;   // the same edits made by XtArpPatternEdit
    ArpPatternView& view() { return ed.view(); }
    // The session's pattern as the edits made it, and Value as the screen says it at the cursor
    void sessionHoldsEdit(const std::string& after) {
        check(p.arpPattern() == want, "the pattern after " + after + " isn't the Pattern Edit's");
        const juce::String value = "Value : " + juce::String(xtArpValueText(want, view().cursor()));
        const std::string said = ed.value().getText().toStdString();
        check(ed.value().getText() == value, "after " + after + " Value says " + said + ", not " + value.toStdString());
    }
    void cursorTo(int line, int step) {
        pressLift(view(), view().cellBounds(line, step).getCentre());
        const std::string where = "line " + std::to_string(line) + " step " + std::to_string(step);
        check(view().cursor().line == line && view().cursor().step == step,
              "a click on " + where + " should put the cursor there");
    }
    void press(int b) {
        ed.valueButton(b).onClick();
        xtArpPress(want, view().cursor(), b);
        sessionHoldsEdit(std::string("button ") + xtArpButtonName(b, xtArpIsCtrl(want, view().cursor().line)));
    }
};

// Each button on a NOTE line, then the same line turned CTRL by its State and each again, the names following
void buttonEdits(PatternCard& c) {
    c.cursorTo(2, 5);
    for (int b = 0; b < kXtArpButtons; ++b) c.press(b);
    c.check(c.ed.valueButton(5).getButtonText() == "Tie", "a NOTE line's last button should be Tie");
    pressLift(c.view(), c.view().stateBounds(2).getCentre());
    xtArpSetState(c.want, 2, 1);
    c.sessionHoldsEdit("line 3's State clicked");
    c.check(c.ed.valueButton(5).getButtonText() == "Clr", "a CTRL line's last button should be Clr");
    for (int b = 0; b < kXtArpButtons; ++b) c.press(b);
    c.press(2);
    pressLift(c.view(), c.view().stateBounds(2).getCentre());
    xtArpSetState(c.want, 2, 0);
    c.sessionHoldsEdit("line 3's State clicked again");
}

// A drag up on the cursor's cell, the wheel on a cell and on a Note/CC, each by INC/DEC's steps
void dragEdits(PatternCard& c) {
    c.cursorTo(7, 12);
    const auto at = c.view().cellBounds(7, 12).getCentre().toFloat();
    LayoutCheck::dragFrom(c.view(), at, at.translated(0.0f, -3.0f * kDragPx));
    xtArpSetCell(c.want, c.view().cursor(), c.want.cell[12][7] + 3 * xtArpCellStep(false));
    c.sessionHoldsEdit("a drag up of three steps");
    wheelBy(c.view(), c.view().cellBounds(9, 30).getCentre(), -1.0f);
    xtArpSetCell(c.want, { 9, 30 }, c.want.cell[30][9] - 1);
    c.check(c.view().cursor().line == 9 && c.view().cursor().step == 30, "the wheel on a cell should move the cursor");
    c.sessionHoldsEdit("the wheel down on line 10 step 31");
    for (int n = 0; n < 5; ++n) wheelBy(c.view(), c.view().noteBounds(4).getCentre(), 1.0f);
    xtArpSetNote(c.want, 4, xtArpNoteOf(c.want, 4) + 5);
    c.sessionHoldsEdit("the wheel up five on line 5's Note/CC");
}

void clearEdits(PatternCard& c) {
    c.cursorTo(2, 9);
    c.ed.lineClear().onClick();
    xtArpClearLine(c.want, 2);
    c.sessionHoldsEdit("LINE CLEAR on line 3");
    c.check(!xtArpPatternSilent(c.p.arpPattern()), "LINE CLEAR should leave line 1's steps");
    c.ed.allClear().onClick();
    xtArpClearAll(c.want);
    c.sessionHoldsEdit("ALL CLEAR");
    c.check(xtArpPatternSilent(c.p.arpPattern()), "ALL CLEAR should leave no step to play");
    c.ed.init().onClick();
    c.want = xtArpInitPattern();
    c.sessionHoldsEdit("INIT");
}

// The edited pattern into the controls the synth plays, and back from a saved session in another processor
void sessionEdits(PatternCard& c) {
    c.cursorTo(11, 3);
    c.press(3);
    pressLift(c.view(), c.view().stateBounds(11).getCentre());
    xtArpSetState(c.want, 11, 1);
    for (int n = 0; n < 40; ++n) wheelBy(c.view(), c.view().noteBounds(11).getCentre(), 1.0f);
    xtArpSetNote(c.want, 11, xtArpNoteOf(c.want, 11) + 40);
    c.sessionHoldsEdit("a CTRL line's controller moved to 100");
    c.check(c.p.currentControls().arpPattern == c.want, "the edited pattern isn't what the synth is given");
    juce::MemoryBlock state;
    c.p.getStateInformation(state);
    VSynthEmuProcessor q;
    q.setStateInformation(state.getData(), (int)state.getSize());
    c.check(q.arpPattern() == c.want, "a saved session should bring the edited pattern back");
    c.check(xtArpNoteOf(q.arpPattern(), 11) == 100, "and a CTRL line's controller past 95 as the patch holds it");
}

}

void patternChecks(LayoutCheck& l, SynthEditor& ed, VSynthEmuProcessor& p, TestChecks& check) {
    auto* page = dynamic_cast<PlayPage*>(ed.pageView(Blocks::kPlayPage));
    ArpPatternEditor* card = page ? page->patternEditor() : nullptr;
    check(card != nullptr, "the PLAY page's pattern should be editable");
    if (card == nullptr) return;
    ed.showPage(Blocks::kPlayPage);
    p.setArpPattern(xtArpInitPattern());
    PatternCard c { *card, p, check, xtArpInitPattern() };
    buttonEdits(c);
    dragEdits(c);
    clearEdits(c);
    sessionEdits(c);
    l.check("the PLAY page, its pattern edited");
}
