// VSynthEmuEditorTest <rom.2.bin> [snapshot folder]: D, the redesigned editor (research/UI_DESIGN.md, "The redesign,
// D"), drawn and checked as it grows (UI step 1).  At its smallest, default and largest sizes its drawing fills the
// window, scaled whole; on each page, and on SYNTH with each block selected, no control is outside its parent, over
// another, without a size or cut short (LayoutCheck), and every control moved sets a parameter.  A press on each page's
// name opens it, on each block selects it, on an effect opens FX.  With a folder: each page, and SYNTH with each block
// selected, drawn into it.  Exit 0 if every check holds.
#include "LayoutCheck.h"
#include "TestChecks.h"
#include "TestMain.h"
#include "TestSnapshot.h"
#include "../Source/UiBlocks.h"
#include "../Source/UiEditor.h"
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

// Each page, and SYNTH with each block selected, into a folder
void snapshots(SynthEditor& ed, const juce::File& dir, TestChecks& check) {
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
}

int editorTest(const juce::File& rom, const juce::File&, const juce::StringArray& rest) {
    TestChecks check { "editor" };
    VSynthEmuProcessor p;
    check(p.setRomFile(rom), "the ROM should load");
    auto owned = std::make_unique<SynthEditor>(p);
    SynthEditor& ed = *owned;
    LayoutCheck l(p, std::move(owned));
    l.root = &ed.drawing();
    sizes(l, ed, check);
    pagesAndBlocks(l, ed);
    presses(l, ed, check);
    if (!rest.isEmpty()) snapshots(ed, juce::File::getCurrentWorkingDirectory().getChildFile(rest[0]), check);
    check(l.errors == 0, std::to_string(l.errors) + " layout errors (said above)");
    return check.summary(std::to_string(l.states) + " states, " + std::to_string(l.checked) + " placements, " +
                         std::to_string(l.reached.size()) + " parameters reached");
}

}

int main(int argc, char** argv) {
    return testMain(argc, argv, "editor", editorTest);
}
