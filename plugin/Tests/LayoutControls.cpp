// LayoutCheck's checks of a control against the parameter it moves (v463, v464; moved out of LayoutCheck.h, which had
// grown past the kit's 250): what moveAll does with each control after moving it.
#include "LayoutCheck.h"

void LayoutCheck::asItsParameter(juce::Component& k, const juce::RangedAudioParameter& moved, double shown,
                                 const juce::String& where) {
    const juce::String at = where + ": " + controlName(&k);
    if (!juce::approximatelyEqual(shownOf(k), shown)) fail(at + " doesn't follow " + moved.paramID + " set back");
    const juce::String learnt = MidiLearn::learnsFor(k);
    if (learnt != moved.paramID) fail(at + " learns for \"" + learnt + "\", not " + moved.paramID);
    if (menus == nullptr) return;
    const auto click = [&] { rightClick(k, *menus); };
    if (changedBy(click)) fail("a right-click on " + at + " moves");
}

double LayoutCheck::shownOf(juce::Component& k) {
    if (auto* s = dynamic_cast<juce::Slider*>(&k)) return s->getValue();
    if (auto* x = dynamic_cast<juce::ComboBox*>(&k)) return x->getSelectedId();
    if (auto* b = dynamic_cast<juce::Button*>(&k)) return b->getToggleState() ? 1.0 : 0.0;
    return 0.0;
}

void LayoutCheck::rightClick(juce::Component& c, MidiLearn::Menus& menus) {
    const auto at = c.getLocalBounds().getCentre().toFloat(), to = at.translated(0.0f, -30.0f);
    const juce::MouseEvent down = mouseAt(c, at, at, rightButton), drag = mouseAt(c, to, at, rightButton);
    c.mouseDown(down);
    menus.mouseDown(down);
    c.mouseDrag(drag);
    menus.mouseDrag(drag);
    c.mouseUp(drag);
}
