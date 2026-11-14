// A rack panel whose controls follow a Type parameter (UiFacedPanel.h; UI step 2)
#include "UiFacedPanel.h"
#include "UiLook.h"
#include "UiParts.h"

FacedPanel::FacedPanel(juce::AudioProcessorValueTreeState& state, const Names& names, int faces)
    : state_(state), knobs_((size_t)faces), typeId_(names.typeId), subs_((size_t)faces) {
    Look::panelName(name_, names.title);
    addAndMakeVisible(name_);
    on_.getProperties().set(Look::power, true);
    Controls::toggle(on_, state, names.switchId, attached_);
    addAndMakeVisible(on_);
    if (Controls::box(type_, state, names.typeId, attached_)) addAndMakeVisible(type_);   // none: one face, no Type
    // Only now: the box's first value, as it is attached, would choose the face before the panel has made its knobs,
    // which then stayed hidden until the Type moved (v196).  The panel shows its face at the end of its own constructor.
    type_.onChange = [this] { refresh(); };
}

void FacedPanel::addSub(int face, const juce::String& id) {
    auto box = std::make_unique<juce::ComboBox>();
    if (!Controls::box(*box, state_, id, attached_)) return;
    addChildComponent(*box);
    subs_[(size_t)face] = std::move(box);
}

void FacedPanel::addKnob(int face, const juce::String& id, const char* name, int dial) {
    if (auto knob = Controls::knob(state_, id, name, dial)) {
        addChildComponent(*knob);
        knobs_[(size_t)face].push_back(std::move(knob));
    }
}

juce::Rectangle<int> FacedPanel::layoutTitle(juce::Rectangle<int> r, const TitleWidths& w) {
    on_.setBounds(r.removeFromLeft(18));
    titleArea_ = r.removeFromLeft(w.name).withTrimmedLeft(4);
    name_.setBounds(titleArea_);
    type_.setBounds(r.removeFromLeft(w.type));
    r.removeFromLeft(6);
    const auto sub = r.removeFromLeft(juce::jmin(w.sub, r.getWidth()));
    for (const auto& box : subs_)
        if (box) box->setBounds(sub);
    return r.withTrimmedLeft(6);
}

void FacedPanel::setSelected(bool selected) {
    if (selected == selected_) return;
    selected_ = selected;
    repaint();
}

void FacedPanel::showFace(int face) {
    face_ = face;
    for (size_t f = 0; f < knobs_.size(); ++f) {
        const bool shown = (int)f == face;
        if (subs_[f]) subs_[f]->setVisible(shown);
        for (const auto& k : knobs_[f]) k->setVisible(shown);
    }
    faceShown();
    repaint();
}

void FacedPanel::refresh() {
    const int face = Controls::faceOf(state_, typeId_, (int)knobs_.size());
    if (face != face_) showFace(face);
}

void FacedPanel::paint(juce::Graphics& g) {
    Look::paintPanel(g, getLocalBounds(), selected_);
    paintDisplay(g);
}

void FacedPanel::mouseDown(const juce::MouseEvent& e) {
    if (titleArea_.contains(e.getPosition()) && onSelect) onSelect();
}
