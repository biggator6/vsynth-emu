#pragma once
// VSynthEmuEditorTest's parts (EditorTest.cpp): the rack's checks, EditorRackTest.cpp (UI step 2), the lower panel's,
// EditorLowerTest.cpp (UI step 3), and the pages', EditorPagesTest.cpp (UI step 4), with what they share
#include "LayoutCheck.h"
#include "TestChecks.h"
#include "../Source/UiBlockKinds.h"
#include "../Source/UiEditor.h"
#include <functional>
#include <vector>

// What the editor has queued for the synth, taken (the keys' notes, the pad's controllers; v201, v202)
inline std::vector<juce::MidiMessage> taken(VSynthEmuProcessor& p) {
    std::vector<juce::MidiMessage> got;
    p.takeFromEditor([&got](const juce::MidiMessage& m) { got.push_back(m); });
    return got;
}

// A page as the kind it should be, shown; none, and said, if it isn't (moved here from EditorPagesTest.cpp in v480,
// the WAVE page's checks taking it too)
template <class T>
T* shownPage(SynthEditor& ed, int page, TestChecks& check) {
    auto* view = dynamic_cast<T*>(ed.pageView(page));
    check(view != nullptr, std::string(Blocks::kPages[(size_t)page]) + " isn't the page it should be");
    if (view != nullptr) ed.showPage(page);
    return view;
}

// The one T in the editor's drawing (its footer, its pad), or none, and said
template <class T>
T* theOne(SynthEditor& ed, const std::string& what, TestChecks& check) {
    const auto all = LayoutCheck::allOf<T>(ed.drawing());
    check(all.size() == 1, "the editor hasn't one " + what);
    return all.size() == 1 ? all.front() : nullptr;
}

// Which instance of its kind a block is, by the prefixes its kind's parameters take (an OSC's, a COSM's); a kind of one
// instance has none to hold its controls to (count 0).  The VC-2's (v453) by `instance`, which says which instance a
// parameter is, or -1 for one of no instance of the kind, or another zone's
struct Owner {
    std::function<juce::String(int)> prefix;
    int count = 0, index = 0;
    std::function<int(const juce::String&)> instance;
};
Owner ownerOf(const Blocks::Block& b);
// How many of a panel's knobs show (its face's)
int visibleKnobs(juce::Component& panel);
// Every control shown under root moved through the real controls; each must move its owner's own parameters alone
void movesOwn(LayoutCheck& l, juce::Component& root, const juce::String& where, const Owner& own, TestChecks& check);
// Each face of a Type in turn: the parameter set, then at(face); the Type left as it was found
void eachFace(VSynthEmuProcessor& p, const juce::String& typeId, int faces, const std::function<void(int)>& at);

// Each OSC's and COSM's panel as the editor opens: its face's knobs shown, as its table gives them
void freshFaces(SynthEditor& ed, TestChecks& check);
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
// The OSC displays (EditorWaveTest.cpp, v200): the analog oscillator's own cycles at the panel's settings, following them;
// a loaded wave's view, holding the file's own extremes; test waves are written in `folder`
void waveChecks(SynthEditor& ed, VSynthEmuProcessor& p, const juce::File& folder, TestChecks& check);
// The WAVE page (EditorWavePageTest.cpp, v479): the Start Offset's line and both loops' ends dragged to the frame, as
// XtSynth reads them, the backward loop's edit kept in the session, the wheel's zoom, the rack's line while it's held
void wavePageChecks(SynthEditor& ed, VSynthEmuProcessor& p, const juce::File& folder, TestChecks& check);
// Its Truncate (EditorWaveCutTest.cpp, v480): the Edit region kept, the loops as the XT leaves them, a cut of a cut,
// the session, a 48 kHz file cut on the XT's import, a SOLO wave cut LITE, Revert
void waveCutChecks(SynthEditor& ed, VSynthEmuProcessor& p, const juce::File& folder, TestChecks& check);
// Its events (EditorWaveEventTest.cpp, v481): the XT's own add and delete in the encode, the law at its edges, the
// refusals said, Next and Prev, the session, a depth's change dropping them
void waveEventChecks(SynthEditor& ed, VSynthEmuProcessor& p, const juce::File& folder, TestChecks& check);
// Its zoom and key (EditorWaveZoomTest.cpp, v484): Fit, - and + about the middle and their readout, the key's marks
// in the colours the view draws them
void waveZoomChecks(SynthEditor& ed, VSynthEmuProcessor& p, const juce::File& folder, TestChecks& check);
// Its playhead (EditorWavePlayTest.cpp, v485): a held note's place on the wave, moving on at the wave's rate, on each of
// the four players, none on an OSC that's off or once the note has ended
void wavePlayChecks(SynthEditor& ed, VSynthEmuProcessor& p, const juce::File& folder, TestChecks& check);
// Its Save Wave (EditorWaveSaveTest.cpp, v489): the session's loops and Truncate written as the XT writes a wave, read
// back, and the saved file loaded back
void waveSaveChecks(SynthEditor& ed, VSynthEmuProcessor& p, const juce::File& folder, TestChecks& check);
// The footer's keyboard (EditorKeysTest.cpp, v201): a key's press, drag and release as their notes, a press sounding
// through processBlock, and the fold
void keyChecks(LayoutCheck& l, SynthEditor& ed, VSynthEmuProcessor& p, TestChecks& check);
// The Time Trip pad (EditorPadTest.cpp, v202): a press's, a drag's and a release's controllers by the assigns as they
// stand, Hold, an assign OFF
void padChecks(LayoutCheck& l, SynthEditor& ed, VSynthEmuProcessor& p, TestChecks& check);
// The arpeggio pattern's card (EditorArpTest.cpp, v468): each edit of the XT's Pattern Edit through it into the session,
// Value and the buttons following the cursor, the clears and INIT, the pattern played and back from a saved session
void patternChecks(LayoutCheck& l, SynthEditor& ed, VSynthEmuProcessor& p, TestChecks& check);
// The VC-2 mode (EditorVc2Test.cpp, v453; research/UI_DESIGN_VC2.md): the header's switch and the session's mode, each
// page and block at three sizes (`sizes`), each zone's, OSC's and COSM's controls on every face moving their own
// parameters alone, after a change of zone too, each Matrix Control's and zone's; and (EditorVc2TextTest.cpp) the
// controls' kinds (d-8), their text (d-9) and the mic's checks, each failing on its own cause
void vc2Checks(LayoutCheck& l, SynthEditor& ed, VSynthEmuProcessor& p, TestChecks& check,
               const std::function<void()>& sizes);
void vc2TextChecks(SynthEditor& ed, VSynthEmuProcessor& p, TestChecks& check);
// Whether c is drawn: a size, and it and every parent up to root shown (the editor is on no desktop, so not isShowing;
// EditorTest.cpp's, from v474 the VC-2's check that it has no INIT too)
bool seen(juce::Component* c, juce::Component& root);
// The patch box's project (EditorPatchBoxTest.cpp, v601): its arrows grey with no patch browsed and at a project's ends,
// ▶ and ◀ loading the next and the one before, the project's line saying where; a project's patches by slot
// MIDI learn (EditorLearnTest.cpp, v463): a knob's menu, Learn arming it, a controller through processBlock learnt and
// the knob following, Forget, the map's rows
void learnChecks(LayoutCheck& l, SynthEditor& ed, VSynthEmuProcessor& p, TestChecks& check);
// Live values (EditorLiveTest.cpp, v464): the firmware's MIDI edits of the patch moving their controls on the PLAY page
void liveChecks(LayoutCheck& l, SynthEditor& ed, VSynthEmuProcessor& p, TestChecks& check);
// and (v464) a modulator as the knob's ring: a Matrix route on a held note, none with nothing added or no note
void ringChecks(LayoutCheck& l, SynthEditor& ed, VSynthEmuProcessor& p, TestChecks& check);
void patchBoxChecks(LayoutCheck& l, VSynthEmuProcessor& p, const juce::File& rom, const juce::File& work,
                    TestChecks& check);
