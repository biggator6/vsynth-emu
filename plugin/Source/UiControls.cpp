// D's controls made from a parameter's ID (UiControls.h; UI step 2; the VC-2's kinds and text, v453)
#include "UiControls.h"
#include "PluginGesture.h"
#include "PluginProcessor.h"
#include "PluginVc2Params.h"
#include "UiVc2Text.h"
#include "Vc2Params.h"

namespace Controls {

namespace {

// A VC-2 parameter's rom.4 ID and range, or none: an ID not the VC-2's, or one made as a stand-in, an instance made
// before rom.4 was in (its range the stand-ins', PluginVc2Params.h), which is a plain knob of its number
struct Vc2Param {
    int id = -1, lo = 0, hi = 0;
};
Vc2Param vc2ParamOf(State& state, const juce::String& id) {
    auto* p = dynamic_cast<juce::AudioParameterInt*>(state.getParameter(id));
    const int rom = Vc2Text::romIdOf(id);
    if (p == nullptr || rom < 0 || vc2ParamDesc(0) == nullptr) return {};
    const Vc2Param v { rom, p->getRange().getStart(), p->getRange().getEnd() };
    return v.lo == kVc2StandInLo && v.hi == kVc2StandInHi ? Vc2Param {} : v;
}

// rom.2's own text routine, for the VC-2 values the V-Synth's routine writes (the patch EQ's), or none before rom.2
XtFxValueText* vsynthText(State& state) {
    auto* p = dynamic_cast<VSynthEmuProcessor*>(&state.processor);
    return p != nullptr ? p->fxCatalog().values.get() : nullptr;
}

constexpr int kMostItems = 1000;   // a VC-2 box's values at most: the Wave Numbers, OFF and 1…999

}

Kind kindFor(State& state, const juce::String& id) {
    auto* p = state.getParameter(id);
    if (dynamic_cast<juce::AudioParameterChoice*>(p) != nullptr) return Kind::box;
    if (dynamic_cast<juce::AudioParameterBool*>(p) != nullptr) return Kind::toggle;
    const Vc2Param v = vc2ParamOf(state, id);
    if (v.id < 0) return Kind::knob;
    switch (Vc2Text::controlKind(v.id, v.lo, v.hi)) {
        case Vc2Text::Kind::box: return Kind::box;
        case Vc2Text::Kind::toggle: return Kind::toggle;
        case Vc2Text::Kind::knob: return Kind::knob;
    }
    return Kind::knob;
}

juce::String editorValueText(State& state, const juce::String& id, int value) {
    const Vc2Param v = vc2ParamOf(state, id);
    if (v.id >= 0) return Vc2Text::editorText(v.id, value, v.lo, v.hi, vsynthText(state));
    auto* p = state.getParameter(id);
    return p != nullptr ? p->getText(p->convertTo0to1((float)value), 0) : juce::String(value);
}

int paramValue(State& state, const juce::String& id) {
    const auto* v = state.getRawParameterValue(id);
    return v != nullptr ? juce::roundToInt(v->load()) : 0;
}

void setValue(State& state, const juce::String& id, int value) {
    if (auto* p = state.getParameter(id)) setAsGesture(*p, (float)value);
}

juce::String currentText(State& state, const juce::String& id) {
    auto* p = state.getParameter(id);
    if (p == nullptr) return {};
    if (vc2ParamOf(state, id).id < 0) return p->getCurrentValueAsText();
    return editorValueText(state, id, juce::roundToInt(state.getRawParameterValue(id)->load()));
}

void showEditorText(juce::Slider& s, State& state, const juce::String& id) {
    if (vc2ParamOf(state, id).id < 0) return;
    s.textFromValueFunction = [&state, id](double v) { return editorValueText(state, id, juce::roundToInt(v)); };
}

bool box(juce::ComboBox& b, State& state, const juce::String& id, Attached& into) {
    juce::StringArray items;
    auto* choice = dynamic_cast<juce::AudioParameterChoice*>(state.getParameter(id));
    const Vc2Param v = vc2ParamOf(state, id);
    if (choice != nullptr) items = choice->choices;
    else if (v.id >= 0 && v.hi - v.lo < kMostItems)
        for (int value = v.lo; value <= v.hi; ++value) items.add(editorValueText(state, id, value));
    if (items.isEmpty()) return false;
    b.clear(juce::dontSendNotification);
    b.addItemList(items, 1);
    b.setTitle(state.getParameter(id)->getName(64));
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

int faceOf(State& state, const juce::String& typeId, int faces) {
    const auto* type = typeId.isEmpty() ? nullptr : state.getRawParameterValue(typeId);
    const float value = type != nullptr ? type->load() : 0.0f;
    return juce::jlimit(0, juce::jmax(0, faces - 1), juce::roundToInt(value));
}

}
