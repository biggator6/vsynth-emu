#pragma once
// VSynthEmuEditorTest's parts (EditorTest.cpp): the rack's checks, EditorRackTest.cpp (UI step 2), the lower panel's,
// EditorLowerTest.cpp (UI step 3), and the pages', EditorPagesTest.cpp (UI step 4), with what they share
#include "LayoutCheck.h"
#include "TestChecks.h"
#include "../Source/UiBlockKinds.h"
#include "../Source/UiEditor.h"
#include <functional>

// Which instance of its kind a block is, by the prefixes its kind's parameters take (an OSC's, a COSM's); a kind of one
// instance has none to hold its controls to (count 0)
struct Owner {
    std::function<juce::String(int)> prefix;
    int count = 0, index = 0;
};
Owner ownerOf(const Blocks::Block& b);
// Every control shown under root moved through the real controls; each must move its owner's own parameters alone
void movesOwn(LayoutCheck& l, juce::Component& root, const juce::String& where, const Owner& own, TestChecks& check);
// Each face of a Type in turn: the parameter set, then at(face); the Type left as it was found
void eachFace(VSynthEmuProcessor& p, const juce::String& typeId, int faces, const std::function<void(int)>& at);

// Every instance's panel, on every face of its Type: its table's knobs all there, and its controls moving its own
// instance's parameters alone; a press on each panel's name selecting its block
void rackChecks(LayoutCheck& l, SynthEditor& ed, VSynthEmuProcessor& p, TestChecks& check);
// The lower panel: each block's views laid out, its tables' rows all made, each instance's controls moving its own
// parameters; a curve's point dragged, a Matrix route appearing and its amount moved, the list's and the overviews'
// presses
void lowerChecks(LayoutCheck& l, SynthEditor& ed, VSynthEmuProcessor& p, TestChecks& check);
// The other pages (EditorPagesTest.cpp, UI step 4): each MSM track, Matrix Control and zone made whole and moving its own
// parameters alone; the FX page's four pages; each effect on the path opening its own
void pagesChecks(LayoutCheck& l, SynthEditor& ed, TestChecks& check);
