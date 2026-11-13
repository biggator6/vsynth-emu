// TEST_UI_LAYOUT=1: the editor's layout, then stop.  At its smallest, default and largest sizes, on every tab, for both OSC types and every COSM type: no control outside its parent, over another or
// without a size, and no text cut short.  Every control is moved, to see which parameter it sets: none may move nothing, and no parameter may be left without a control, bar the engine's three, kept
// unread for old sessions (v191).  (buildLayout drops what it doesn't place, so a control left out of it goes quietly.)  And each tab by a click, and the joined buttons both ways.  =list: the parameters with no control, and those with.
// Moved from PluginTest.cpp's main and split by what each part does (v193): the checks are LayoutCheck.h's, the sweeps
// here.  And the MSM tab's tracks, one at a time as the Matrix's Controls, each track's controls on its own parameters.
#include "LayoutTest.h"
#include "LayoutCheck.h"
#include "TestControls.h"
#include "../Source/XtBuildId.h"

namespace {

// No editor has shown these (v143's didn't either): the engine's three (v191: unread, kept for old sessions); Scale Tune; the COSMs' second envelope, the first's velocity and key follow, their LFOs' fade and offset
bool hostOnly(const juce::String& id) {
    if (id == "pitchShift" || id == "timeStretch" || id == "formantShift" || id.startsWith("scaleTune")) return true;
    for (const char* env : { "itchEnv", "imeEnv", "ormantEnv" })
        for (const char* knob : { "VelCurve", "VelSens", "AttackSens", "DecaySens", "ReleaseSens", "TimeKF" })
            if (id.endsWith(juce::String(env) + knob)) return true;
    const juce::String c = id.startsWith("cosm2") ? id.substring(5) : id.startsWith("cosm") ? id.substring(4) : juce::String();
    return c.startsWith("EnvB") || c == "EnvAVelCurve" || c == "EnvAVelSens" || c == "EnvAAttackSens" || c == "EnvADecaySens" || c == "EnvAReleaseSens" || c == "EnvATimeKF"
        || c == "LfoOffset" || c == "LfoDelay" || c == "LfoFadeMode" || c == "LfoFade";
}

} // namespace

// The MSM tab a track at a time, as the Matrix's Controls (v193): what each track's controls reach first is its own
void LayoutCheck::msmTracks() {
    editor->showTab(6);
    for (int t = 0; t < 4; ++t) {
        boxOf("Track 1")->setSelectedItemIndex(t, juce::sendNotificationSync);
        const std::set<juce::String> before = reached;
        const juce::String where = "MSM track " + juce::String(t + 1), own = "msm" + juce::String(t + 1);
        moveAll(where);
        for (const juce::String& id : reached)
            if (!before.count(id) && !id.startsWith(own)) fail(where + "'s controls move " + id);
    }
}

// Every control on every tab, by the OSC, its type and the COSM; the Effects pages, the Matrix's Controls, the assigns,
// the MSM's tracks
void LayoutCheck::moveEverywhere() {
    for (int k = 0; k < 2; ++k) {
        boxOf("OSC 1")->setSelectedId(k + 1, juce::sendNotificationSync);
        for (int type = 0; type < 2; ++type) {
            setParam(p, VSynthEmuProcessor::oscParamId(k, "oscType"), (float)type);
            for (int tab = 0; tab <= VSynthEmuEditor::kEffectsTab; ++tab) { editor->showTab(tab); moveAll("OSC " + juce::String(k + 1) + " type " + juce::String(type) + " tab " + juce::String(tab)); }
        }
    }
    boxOf("OSC 1")->setSelectedId(1, juce::sendNotificationSync);
    for (int k = 0; k < 2; ++k) {
        boxOf("COSM 1")->setSelectedId(k + 1, juce::sendNotificationSync);
        for (int tab = 0; tab <= VSynthEmuEditor::kEffectsTab; ++tab) { editor->showTab(tab); moveAll("COSM " + juce::String(k + 1) + " tab " + juce::String(tab)); }   // the COSM's envelope is on another tab
        editor->showTab(3);
        for (int type = 0; type < cosmTypes; ++type) {
            setParam(p, k ? "cosm2Type" : "cosmType", (float)type);
            moveAll("COSM " + juce::String(k + 1) + " type " + juce::String(type));
        }
    }
    boxOf("COSM 1")->setSelectedId(1, juce::sendNotificationSync);
    editor->showTab(VSynthEmuEditor::kEffectsTab);
    for (int page = 0; page < 4; ++page) { panel->showPage(page); moveAll("Effects page " + juce::String(page)); }
    panel->showPage(0);
    editor->showTab(7);   // the Matrix (the MSM's tab came before it, v193)
    for (int m = 0; m < 8; ++m) { boxOf("Control 1")->setSelectedItemIndex(m, juce::sendNotificationSync); moveAll("Matrix Control " + juce::String(m + 1)); }
    for (int a = 0; a < VSynthEmuProcessor::kAssigns; ++a) { boxOf(VSynthEmuProcessor::assignName(0))->setSelectedItemIndex(a, juce::sendNotificationSync); moveAll(juce::String(VSynthEmuProcessor::assignName(a)) + " assign"); }
    msmTracks();
}

// How many parameters a control reached; each one no control reached said, as missing or, with "list", as a line
int LayoutCheck::coverage(const char* ui) {
    int have = 0;
    for (auto* r : params) {
        if (reached.count(r->paramID)) ++have;
        else if (juce::String(ui) == "list") std::printf("no control: %s\n", r->paramID.toRawUTF8());
        else if (!hostOnly(r->paramID)) fail("no control sets " + r->paramID);
    }
    return have;
}

// The source bar's two buttons, which no parameter would miss
void LayoutCheck::chrome() {
    for (const char* text : { "Load WAV...", "Load Patch..." }) {
        bool there = false;
        for (auto* child : ed->getChildren())
            if (auto* b = dynamic_cast<juce::TextButton*>(child))
                if (b->getButtonText() == text && b->isVisible() && !b->getBounds().isEmpty()) there = true;
        if (!there) fail(juce::String(text) + " isn't on the editor");
    }
    // Which build this is, on the header, whole
    {
        const juce::String want = juce::String(xtBuildId()) + "  " + xtBuildTime();
        bool there = false;
        for (auto* child : ed->getChildren())
            if (auto* l = dynamic_cast<juce::Label*>(child))
                if (l->getText() == want && l->isVisible() && !l->getBounds().isEmpty()) there = true;
        if (!there) fail("the build, " + want + ", isn't on the editor");
    }
}

// The Effects tab: each page, and each block's types, as what's on a page follows them
void LayoutCheck::effectsAt(const juce::String& at) {
    editor->showTab(VSynthEmuEditor::kEffectsTab);
    static const char* const typeOf[] = { "fxMfxType", "fxChoType", "fxRevType" };
    for (int page = 0; page < 4; ++page) {
        const int types = page < 3 ? p.apvts.getParameter(typeOf[page])->getNumSteps() : 1;
        for (int type = 0; type < types; ++type) {
            if (page < 3) setParam(p, typeOf[page], (float)type);
            panel->showPage(page);
            const juce::String where = at + " Effects page " + juce::String(page) + " type " + juce::String(type);
            check(where);
            checkList(where);
            if (view->getHeight() < 15 * 24) fail(where + ": the list shows only " + juce::String(view->getHeight() / 24) + " rows");
        }
        if (page < 3) setParam(p, typeOf[page], 0.0f);
    }
    panel->showPage(0);
}

// The layout, at each size; left at the smallest
void LayoutCheck::atSizes() {
    const juce::ComponentBoundsConstrainer* limits = ed->getConstrainer();
    const juce::Point<int> sizes[] = { { limits->getMinimumWidth(), limits->getMinimumHeight() }, { ed->getWidth(), ed->getHeight() }, { limits->getMaximumWidth(), limits->getMaximumHeight() } };
    for (const auto& size : sizes) {
        ed->setSize(size.x, size.y);
        const juce::String at = juce::String(size.x) + "x" + juce::String(size.y);
        for (int type = 0; type < 2; ++type) {
            setParam(p, "oscType", (float)type);
            for (int tab = 0; tab <= VSynthEmuEditor::kEffectsTab; ++tab) { editor->showTab(tab); check(at + " tab " + juce::String(tab) + " OSC type " + juce::String(type)); }
        }
        editor->showTab(3);
        for (int type = 0; type < cosmTypes; ++type) {
            setParam(p, "cosmType", (float)type);
            check(at + " COSM type " + juce::String(type));
        }
        setParam(p, "cosmType", 0.0f);
        effectsAt(at);
    }
    ed->setSize(sizes[0].x, sizes[0].y);
}

// Each tab by a click, in order, at the smallest width; and the last one ending inside the bar, so a click at its right end
// opens none (v195: ten tabs ran Effects past the end at the smallest width, where a click still reached it)
void LayoutCheck::tabClicks() {
    editor->showTab(0);
    if (juce::Component* bar = ed->findChildWithID("tabs")) {
        const auto click = [bar](int x) {
            pressAt(*bar, { (float)x, (float)bar->getHeight() / 2.0f });
        };
        int seen = 0;
        for (int x = 0; x < bar->getWidth(); x += 4) {
            click(x);
            if (editor->currentTab() == seen + 1) ++seen;
            else if (editor->currentTab() != seen)
                fail("the click at x " + juce::String(x) + " opens tab " + juce::String(editor->currentTab()) +
                     " after tab " + juce::String(seen));
        }
        if (seen != VSynthEmuEditor::kEffectsTab) fail("clicks reach only as far as tab " + juce::String(seen));
        editor->showTab(0);
        click(bar->getWidth() - 4);
        if (editor->currentTab() != 0)
            fail("the last tab runs past the tab bar's end, at " + juce::String(ed->getWidth()) + " wide");
    } else fail("no tab bar");
}

// The joined buttons drive their boxes, and follow them
void LayoutCheck::joined() {
    for (const char* first : { "OSC 1", "COSM 1" }) {
        juce::Button* one = buttonOf(first);
        juce::Button* two = buttonOf(juce::String(first).upToFirstOccurrenceOf(" ", false, false) + " 2");
        juce::ComboBox* box = boxOf(first);
        if (!one || !two || !box) { fail(juce::String("no selector for ") + first); continue; }
        two->setToggleState(true, juce::sendNotificationSync);
        if (box->getSelectedId() != 2) fail(juce::String("the second button of ") + first + " doesn't set its box");
        box->setSelectedId(1, juce::sendNotificationSync);
        if (!one->getToggleState()) fail(juce::String("the first button of ") + first + " doesn't follow its box");
    }
    if (juce::Button* mono = buttonOf("MONO"); mono && buttonOf("POLY")) {
        const bool poly = buttonOf("POLY")->getToggleState();
        juce::Button* other = poly ? mono : buttonOf("POLY");
        if (!changedBy([other] { other->setToggleState(true, juce::sendNotificationSync); })) fail("MONO/POLY set no parameter");
        if (buttonOf("POLY")->getToggleState() != poly) fail("MONO/POLY doesn't follow its parameter");
    } else fail("no MONO/POLY");
    if (juce::Button* chorus = buttonOf("Chorus"); chorus && buttonOf("System")) {
        chorus->setToggleState(true, juce::sendNotificationSync);
        if (panel->page() != 1) fail("the Effects tab's Chorus button doesn't open its page");
        panel->showPage(3);
        if (!buttonOf("System")->getToggleState()) fail("the Effects tab's System button doesn't follow its page");
    } else fail("no page buttons on the Effects tab");
}

int uiLayoutTest(VSynthEmuProcessor& p, const char* ui) {
    LayoutCheck l(p);
    if (!l.view) {
        std::fprintf(stderr, "layout: no effects' panel\n");
        return 1;
    }
    l.moveEverywhere();
    const int have = l.coverage(ui);
    if (juce::String(ui) == "list") {
        for (const auto& id : l.reached) std::printf("control: %s\n", id.toRawUTF8());
        return 0;
    }
    l.chrome();
    l.atSizes();
    l.tabClicks();
    l.joined();
    if (l.errors) {
        std::fprintf(stderr, "layout: %d errors\n", l.errors);
        return 1;
    }
    std::printf("the layout: %d states at 3 sizes, %d placements, tabs by click; %d of %d parameters have a control\n",
                l.states, l.checked, have, (int)l.params.size());
    return 0;
}
