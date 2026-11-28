// Controls that follow their values live (v464; research/NEXT_ISSUES.md item 2, the user's d-5): a stored value the
// firmware's MIDI edits (rom.2's OS part: CC 5, 65, 126 and 127, RPN 0, 1 and 2) moves its own control on the PLAY page
// once the message thread has handed the edit on, as a host's automation does (which LayoutCheck::asItsParameter holds
// every control to); and its control stays where it was until then, the synth already playing the edit.  Made with
// nothing playing, no wave loaded and no OSC needing none, as a fresh instance has it: the edits are made whatever
// plays, and only this check can show them made while the players don't (the merge of vc2-mode into the wave editor,
// whose checks leave a wave encoded and playing).
#include "EditorTest.h"
#include "TestControls.h"
#include "TestMidi.h"
#include "../Source/PluginParamIds.h"
#include "../Source/UiMidiLearn.h"
#include <utility>

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
    const bool pcm = juce::roundToInt(p.apvts.getRawParameterValue(Params::kOscType)->load()) == Params::kOscTypePcm;
    const bool osc2 = p.apvts.getRawParameterValue(Params::oscId(1, Params::kOscSwitch))->load() >= 0.5f;
    check(!p.hasFileLoaded(0) && !p.hasFileLoaded(1) && pcm && !osc2, "the live check should run with nothing "
          "playing (EditorTest.cpp: before the wave checks), or it can't show the edits made while nothing plays");
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

namespace {
// The knob's ring as proportions of its range, none as (-1, -1)
std::pair<float, float> knobRing(juce::Slider& dial) {
    const auto& props = dial.getProperties();
    if (!props.contains(Look::modLo)) return { -1.0f, -1.0f };
    return { (float)props[Look::modLo], (float)props[Look::modHi] };
}

// Whether c shows under root: it and every parent up to root visible (a face's knobs are there on every face)
bool shownUnder(juce::Component& c, juce::Component& root) {
    for (juce::Component* at = &c; at != nullptr && at != &root; at = at->getParentComponent())
        if (!at->isVisible()) return false;
    return true;
}
}  // namespace

// v464 (d-5): a modulator as the knob's ring.  TestMidi.h's levelRoute from CC 1: Matrix Control 1 onto OSC 1's Level
// at +63, Level stored at 64, on ANALOG; a note held.  CC 1 at 127 rings Level's knob from 64 up by the newest note's
// sum, the knob itself staying on 64; CC 1 at 0 takes the ring away; with the note let go and ended, none.
void ringChecks(LayoutCheck& l, SynthEditor& ed, VSynthEmuProcessor& p, TestChecks& check) {
    ed.showPage(Blocks::kSynthPage);
    const auto route = levelRoute(p, 1);
    auto* knob = l.firstOf<LearnSlider>([&](LearnSlider& s) {
        return MidiLearn::learnsFor(s) == "oscLevel" && shownUnder(s, ed.drawing());
    });
    check(knob != nullptr, "OSC 1's Level knob should show");
    if (knob == nullptr) return;
    const float at64 = p.apvts.getParameter("oscLevel")->getValue();
    holdBlocks(p, { controller(1, 127), juce::MidiMessage::noteOn(1, 60, (juce::uint8)100) }, 4);
    check(p.modulation().sounding(), "the note should sound, for the rings to read");
    const int sum = p.modulation().matrix(xtmx::OscLevel);
    check(sum > 0, "CC 1 at 127 through Sens +63 should add to OSC 1's Level, not " + std::to_string(sum));
    ed.showRings();
    const auto [lo, hi] = knobRing(*knob);
    check(juce::approximatelyEqual(lo, at64) && hi > lo, "a Matrix route on a held note should ring the knob up");
    check(juce::approximatelyEqual(knob->getValue(), 64.0), "the knob itself on what's stored");
    holdBlocks(p, { controller(1, 0) }, 4);
    ed.showRings();
    check(knobRing(*knob).first < 0.0f, "nothing added, no ring");
    holdBlocks(p, { controller(1, 127), juce::MidiMessage::noteOff(1, 60) }, 400);
    ed.showRings();
    check(knobRing(*knob).first < 0.0f, "no note sounding, no ring");
    toDefaults(p, route);
}
