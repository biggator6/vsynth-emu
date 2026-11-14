// VSynthEmuEditorTest <rom.2.bin> [snapshot folder | list]: the plugin's editor, D (research/UI_DESIGN.md), drawn and
// checked; from v196 in TEST_UI_LAYOUT's place.  At its smallest, default and largest sizes its drawing fills the
// window, scaled whole; on each page, and on SYNTH with each block selected, no control is outside its parent, over
// another, without a size or cut short (LayoutCheck), and every control moved sets a parameter.  A press on each page's
// name opens it, on each block selects it, on an effect opens FX; the header's build and buttons, and a patch's note.
// The rack's checks (EditorRackTest.cpp), the lower panel's (EditorLowerTest.cpp) and the pages'
// (EditorPagesTest.cpp).  With a folder: each page, SYNTH with each block selected, and each of the lower panel's views,
// drawn into it; with "list", the parameters no control reaches.  Exit 0 if every check holds.
#include "EditorTest.h"
#include "LayoutCheck.h"
#include "TestChecks.h"
#include "TestMain.h"
#include "TestControls.h"
#include "TestSnapshot.h"
#include "../Source/PluginParamIds.h"
#include "../Source/UiBlocks.h"
#include "../Source/UiEditor.h"
#include "../Source/XtBuildId.h"
#include <cstdlib>

namespace {

// The drawing filling the window, scaled whole, at each size the window takes
void sizes(LayoutCheck& l, SynthEditor& ed, TestChecks& check) {
    const juce::ComponentBoundsConstrainer* limits = ed.getConstrainer();
    const juce::Point<int> at[] = { { limits->getMinimumWidth(), limits->getMinimumHeight() },
                                    { Look::width, Look::height },
                                    { limits->getMaximumWidth(), limits->getMaximumHeight() } };
    for (const auto& size : at) {
        ed.setSize(size.x, size.y);
        const juce::String where = juce::String(size.x) + "x" + juce::String(size.y);
        const juce::Rectangle<int> drawn = ed.drawing().getBoundsInParent();
        check(std::abs(drawn.getWidth() - ed.getWidth()) <= 1 && std::abs(drawn.getHeight() - ed.getHeight()) <= 1,
              (where + ": the drawing is " + drawn.toString()).toStdString());
        l.check(where);
    }
    ed.setSize(Look::width, Look::height);
}

// Each page, then SYNTH with each block selected: the layout's checks, and every control moved
void pagesAndBlocks(LayoutCheck& l, SynthEditor& ed) {
    for (int page = 0; page < (int)Blocks::kPages.size(); ++page) {
        ed.showPage(page);
        l.check(Blocks::kPages[(size_t)page]);
        l.moveAll(Blocks::kPages[(size_t)page]);
    }
    ed.showPage(Blocks::kSynthPage);
    for (int block = 0; block < Blocks::kCount; ++block) {
        ed.select(block);
        l.check("SYNTH, block " + juce::String(block));
    }
}

// The route each press takes: a page's name to its page, a block to its selection, an effect to FX
void presses(LayoutCheck& l, SynthEditor& ed, TestChecks& check) {
    PageBar* bar = l.firstOf<PageBar>([](PageBar&) { return true; });
    PathBar* path = l.firstOf<PathBar>([](PathBar&) { return true; });
    check(bar != nullptr && path != nullptr, "no page bar or no signal path");
    if (!bar || !path) return;
    const auto tabs = bar->tabs();
    for (size_t page = 0; page < tabs.size(); ++page) {
        LayoutCheck::pressAt(*bar, tabs[page].getCentre().toFloat());
        check(ed.page() == (int)page, std::string("a press on ") + Blocks::kPages[page] + " doesn't open it");
    }
    ed.showPage(Blocks::kSynthPage);
    const PathBar::Chips chips = path->laidOut();
    for (size_t block = 0; block < chips.blocks.size(); ++block) {
        LayoutCheck::pressAt(*path, chips.blocks[block].r.getCentre().toFloat());
        const std::string name = chips.blocks[block].name.toStdString();
        check(ed.selected() == (int)block, "a press on " + name + " doesn't select it");
    }
    LayoutCheck::pressAt(*path, chips.effects.front().r.getCentre().toFloat());
    check(ed.page() == Blocks::kFxPage, "a press on an effect doesn't open FX");
    ed.showPage(Blocks::kSynthPage);
}

// Whether c is drawn: a size, and it and every parent up to root shown (the editor is on no desktop, so not isShowing)
bool seen(juce::Component* c, juce::Component& root) {
    if (c == nullptr || c->getBounds().isEmpty()) return false;
    for (juce::Component* at = c; at != nullptr && at != &root; at = at->getParentComponent())
        if (!at->isVisible()) return false;
    return true;
}

// A patch's note as a session brings it back, and the header told (its timer's refresh)
void withNote(VSynthEmuProcessor& p, SynthEditor& ed, const juce::String& note) {
    p.apvts.state.setProperty("patchNote", note, nullptr);
    juce::MemoryBlock state;
    p.getStateInformation(state);
    p.setStateInformation(state.getData(), (int)state.getSize());
    for (Header* h : LayoutCheck::allOf<Header>(ed.drawing())) h->refresh();
}

// What no parameter would miss (v196, as TEST_UI_LAYOUT had it of the tabbed editor): the build on the header, whole;
// Browse..., ROM... and each PCM OSC's Load...; what a patch asks for that isn't here, under its name, and nothing there
// when it misses nothing
void chrome(LayoutCheck& l, SynthEditor& ed, VSynthEmuProcessor& p, TestChecks& check) {
    const juce::String build = juce::String(xtBuildId()) + "  " + xtBuildTime();
    const auto labelOf = [&l](const juce::String& text) {
        return l.firstOf<juce::Label>([&text](juce::Label& x) { return x.getText() == text; });
    };
    check(seen(labelOf(build), ed), ("the build, " + build + ", isn't on the header").toStdString());
    for (const char* text : { "Browse...", "ROM..." })
        check(seen(l.buttonOf(text), ed), std::string(text) + " isn't on the header");
    for (int osc = 0; osc < Blocks::kOscs; ++osc) setParam(p, Params::oscId(osc, Params::kOscType), 1.0f);
    for (FacedPanel* panel : LayoutCheck::allOf<FacedPanel>(ed.drawing())) panel->refresh();   // as the timer does
    int loads = 0;
    for (juce::TextButton* b : LayoutCheck::allOf<juce::TextButton>(ed.drawing()))
        if (b->getButtonText() == "Load..." && seen(b, ed)) ++loads;
    check(loads == Blocks::kOscs, "each PCM OSC should have its Load..., and " + std::to_string(loads) + " do");
    const juce::String note = "not here: no wave 12";
    withNote(p, ed, "no wave 12");
    check(seen(labelOf(note), ed), "a patch's note isn't under its name");
    l.check("a patch's note");
    withNote(p, ed, {});
    check(!seen(labelOf(note), ed), "a patch's note stays when nothing is missing");
}

// Each page, and SYNTH with each block selected, into a folder
void snapshots(SynthEditor& ed, const juce::File& dir, TestChecks& check) {
    check(dir.createDirectory().wasOk(), "can't make " + dir.getFullPathName().toStdString());
    const auto shoot = [&](const juce::String& name) {
        check(writeSnapshot(ed, dir.getChildFile(name + ".png")), "can't write " + name.toStdString());
    };
    for (int page = 0; page < (int)Blocks::kPages.size(); ++page) {
        ed.showPage(page);
        shoot(juce::String("page-") + Blocks::kPages[(size_t)page]);
    }
    ed.showPage(Blocks::kSynthPage);
    for (int block = 0; block < Blocks::kCount; ++block) {
        ed.select(block);
        shoot("synth-block" + juce::String(block));
    }
    ed.select(Blocks::kFirstSelected);
    const juce::StringArray views = ed.lower().list().names();
    for (int v = 0; v < views.size(); ++v) {
        ed.lower().showView((LowerPanel::View)v);
        shoot("synth-view-" + views[v]);
    }
    ed.lower().showView(LowerPanel::View::selected);
}

// Every parameter with a control that has moved it, but those no editor has shown (the engine's three, unread and kept
// for old sessions, and Scale Tune); with `list`, the parameters no control reaches, said.  How many have one.
int coverage(LayoutCheck& l, TestChecks& check, bool list) {
    const juce::StringArray unshown { "pitchShift", "timeStretch", "formantShift" };
    int have = 0;
    for (auto* r : l.params) {
        const juce::String& id = r->paramID;
        if (l.reached.count(id) != 0) ++have;
        else if (list) std::printf("no control: %s\n", id.toRawUTF8());
        else check(unshown.contains(id) || id.startsWith("scaleTune"), ("no control sets " + id).toStdString());
    }
    return have;
}

int editorTest(const juce::File& rom, const juce::File&, const juce::StringArray& rest) {
    TestChecks check { "editor" };
    VSynthEmuProcessor p;
    check(p.setRomFile(rom), "the ROM should load");
    auto owned = std::make_unique<SynthEditor>(p);
    SynthEditor& ed = *owned;
    LayoutCheck l(p, std::move(owned));
    l.root = &ed.drawing();
    freshFaces(ed, check);
    sizes(l, ed, check);
    pagesAndBlocks(l, ed);
    presses(l, ed, check);
    chrome(l, ed, p, check);
    rackChecks(l, ed, p, check);
    lowerChecks(l, ed, p, check);
    pagesChecks(l, ed, check);
    const int reached = coverage(l, check, rest[0] == "list");
    if (rest[0] != "list" && !rest.isEmpty())
        snapshots(ed, juce::File::getCurrentWorkingDirectory().getChildFile(rest[0]), check);
    check(l.errors == 0, std::to_string(l.errors) + " layout errors (said above)");
    return check.summary(std::to_string(l.states) + " states, " + std::to_string(l.checked) + " placements, " +
                         std::to_string(reached) + " of " + std::to_string(l.params.size()) +
                         " parameters have a control");
}

}

int main(int argc, char** argv) {
    return testMain(argc, argv, "editor", editorTest);
}
