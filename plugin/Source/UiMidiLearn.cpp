// MIDI learn from a control (UiMidiLearn.h, v463)
#include "UiMidiLearn.h"
#include "PluginProcessor.h"
#include "UiMidiMap.h"
#include <utility>

namespace {
const juce::Identifier kLearnId { "midiLearnId" };

// The control a press was on: the one carrying the property, the press's component or a parent of it
juce::Component* learner(juce::Component* c) {
    for (; c != nullptr; c = c->getParentComponent())
        if (c->getProperties().contains(kLearnId)) return c;
    return nullptr;
}

// A switch's own press came first: kept from toggling when the button comes up
void keepUp(juce::Component& control) {
    if (auto* b = dynamic_cast<juce::Button*>(&control)) b->setState(juce::Button::buttonNormal);
}
}  // namespace

void LearnSlider::mouseDown(const juce::MouseEvent& e) {
    menuPress_ = e.mods.isPopupMenu();
    if (!menuPress_) juce::Slider::mouseDown(e);
}

void LearnSlider::mouseDrag(const juce::MouseEvent& e) {
    if (menuPress_) return;
    juce::Slider::mouseDrag(e);
}

void LearnSlider::mouseUp(const juce::MouseEvent& e) {
    const bool menu = std::exchange(menuPress_, false);
    if (!menu) juce::Slider::mouseUp(e);
}

namespace MidiLearn {

void attach(juce::Component& control, const juce::String& id) { control.getProperties().set(kLearnId, id); }

juce::String learnsFor(const juce::Component& control) { return control.getProperties()[kLearnId].toString(); }

juce::PopupMenu menuFor(VSynthEmuProcessor& p, const juce::String& id) {
    const PluginMidiLearn& learnt = p.midiIn().learn;
    const int index = p.midiIn().edits.indexOf(id);
    juce::PopupMenu m;
    const bool waiting = index >= 0 && learnt.armed() == index;
    m.addItem(waiting ? cancel : learn, waiting ? "Cancel learning" : "Learn MIDI CC", index >= 0);
    if (const int cc = learnt.controllerOf(index); cc >= 0) m.addItem(forget, "Forget CC " + juce::String(cc));
    m.addSeparator();
    m.addItem(map, "MIDI map...");
    return m;
}

void choose(VSynthEmuProcessor& p, juce::Component& control, const juce::String& id, int item) {
    PluginMidiLearn& learnt = p.midiIn().learn;
    const int index = p.midiIn().edits.indexOf(id);
    switch (item) {
        case learn: learnt.arm(index); break;
        case cancel: learnt.arm(-1); break;
        case forget: learnt.forget(index); break;
        case map: MidiMapView::open(p, control); break;
        default: break;
    }
}

juce::String learning(VSynthEmuProcessor& p) {
    const auto* param = p.midiIn().edits.param(p.midiIn().learn.armed());
    return param ? "MIDI learn: move a controller for " + param->getName(64) + " (right-click it to cancel)" : "";
}

Menus::~Menus() {
    if (editor_ != nullptr) editor_->removeMouseListener(this);
}

void Menus::listenTo(juce::Component& editor) {
    editor_ = &editor;
    editor.addMouseListener(this, true);
}

void Menus::mouseDown(const juce::MouseEvent& e) {
    if (!e.mods.isPopupMenu()) return;
    juce::Component* control = learner(e.eventComponent);
    if (control == nullptr) return;
    keepUp(*control);
    const juce::String id = learnsFor(*control);
    juce::Component::SafePointer<juce::Component> safe(control);
    VSynthEmuProcessor& p = processor_;
    const auto chosen = [&p, safe, id](int item) {
        if (safe != nullptr && item > 0) choose(p, *safe, id, item);
    };
    juce::PopupMenu menu = menuFor(p, id);
    if (shower_) shower_(menu, *control, chosen);
    else menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(control), chosen);
}

void Menus::mouseDrag(const juce::MouseEvent& e) {
    if (juce::Component* control = learner(e.eventComponent); control != nullptr && e.mods.isPopupMenu())
        keepUp(*control);
}

}  // namespace MidiLearn
