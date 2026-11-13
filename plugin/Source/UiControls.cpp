// D's controls made from a parameter's ID (UiControls.h; UI step 2)
#include "UiControls.h"

namespace Controls {

bool box(juce::ComboBox& b, State& state, const juce::String& id, Attached& into) {
    auto* choice = dynamic_cast<juce::AudioParameterChoice*>(state.getParameter(id));
    if (choice == nullptr) return false;
    b.clear(juce::dontSendNotification);
    b.addItemList(choice->choices, 1);
    b.setTitle(choice->getName(64));
    into.boxes.push_back(std::make_unique<State::ComboBoxAttachment>(state, id, b));
    return true;
}

bool toggle(juce::Button& b, State& state, const juce::String& id, Attached& into) {
    if (state.getParameter(id) == nullptr) return false;
    into.switches.push_back(std::make_unique<State::ButtonAttachment>(state, id, b));
    return true;
}

std::unique_ptr<Knob> knob(State& state, const juce::String& id, const juce::String& name, int dialSize) {
    if (state.getParameter(id) == nullptr) return nullptr;
    return std::make_unique<Knob>(state, id, name, dialSize);
}

}
