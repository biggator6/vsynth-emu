// Controls that follow their values live (v464; research/NEXT_ISSUES.md item 2, the user's d-5): a stored value the
// firmware's MIDI edits (rom.2's OS part: CC 5, 65, 126 and 127, RPN 0, 1 and 2) moves its own control on the PLAY page
// once the message thread has handed the edit on, as a host's automation does (which LayoutCheck::asItsParameter holds
// every control to); and its control stays where it was until then, the synth already playing the edit.
#include "EditorTest.h"
#include "TestMidi.h"
#include "../Source/UiMidiLearn.h"

namespace {
// What the control for a parameter shows, by the parameter it learns for; -1 for none
double shownFor(LayoutCheck& l, const juce::String& id) {
    auto* c = l.firstOf<juce::Component>([&](juce::Component& k) {
        return MidiLearn::learnsFor(k) == id && isControl(&k);
    });
    return c != nullptr ? LayoutCheck::shownOf(*c) : -1.0;
}
}  // namespace

void liveChecks(LayoutCheck& l, SynthEditor& ed, VSynthEmuProcessor& p, TestChecks& check) {
    ed.showPage(Blocks::kPlayPage);
    const double timeWas = shownFor(l, "portaTime");
    playMessages(p, { controller(5, 100), controller(65, 127), controller(126, 0), controller(101, 0),
                      controller(100, 0), controller(6, 7), controller(100, 2), controller(6, 76) });
    check(juce::approximatelyEqual(shownFor(l, "portaTime"), timeWas), "a control should wait for the hand-on");
    p.midiIn().edits.flush();
    check(juce::approximatelyEqual(shownFor(l, "portaTime"), 100.0), "Portamento Time's knob should follow CC 5");
    check(juce::approximatelyEqual(shownFor(l, "portamento"), 1.0), "Portamento's switch CC 65");
    check(juce::approximatelyEqual(shownFor(l, "monoPoly"), 1.0), "Mono/Poly's box CC 126, MONO its first");
    check(juce::approximatelyEqual(shownFor(l, "bendRangeUp"), 7.0)
              && juce::approximatelyEqual(shownFor(l, "bendRangeDown"), 7.0), "the Bend Ranges' knobs RPN 0");
    check(juce::approximatelyEqual(shownFor(l, "patchCoarse"), 12.0), "Coarse Tune's knob RPN 2");
    playMessages(p, { controller(5, 20), controller(65, 0), controller(127, 0), controller(100, 0), controller(6, 2),
                      controller(100, 2), controller(6, 64) });
    p.midiIn().edits.flush();
    check(juce::approximatelyEqual(shownFor(l, "monoPoly"), 2.0), "and back: CC 127, POLY");
    ed.showPage(Blocks::kSynthPage);
}
