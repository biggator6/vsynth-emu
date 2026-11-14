// A block's DETAILS (UiDetails.h; UI step 3)
#include "UiDetails.h"
#include "UiLook.h"

namespace {

constexpr int kDial = 30, kCell = 64, kGap = 8, kSwitchH = 24;

// A row's control as its parameter is, and how many cells across it takes: a choice's box and a switch two, a knob one;
// none if the ID names no parameter (a table's mistake, which the editor's test reports)
std::unique_ptr<juce::Component> controlFor(Controls::State& s, const juce::String& id, const char* name,
                                            Controls::Attached& attached, int& cells) {
    auto* p = s.getParameter(id);
    if (p == nullptr) return nullptr;
    cells = 2;
    if (dynamic_cast<juce::AudioParameterChoice*>(p) != nullptr) return std::make_unique<Field>(s, id, name);
    if (dynamic_cast<juce::AudioParameterBool*>(p) != nullptr) {
        auto t = std::make_unique<juce::ToggleButton>(name);
        Controls::toggle(*t, s, id, attached);
        return t;
    }
    cells = 1;
    return Controls::knob(s, id, name, kDial);
}

}

DetailsView::DetailsView(Controls::State& state, const Blocks::Block& b)
    : state_(state), typeId_(Blocks::typeIdOf(b)) {
    const Blocks::BlockKind& kind = Blocks::kindOf(b);
    for (int f = 0; f < kind.faces; ++f) {
        Face face;
        for (const Controls::KnobRow& row : kind.details[f].rows) {
            if (row.id == nullptr) break;
            int cells = 1;
            auto c = controlFor(state, Blocks::blockParam(b, row.id), row.name, attached_, cells);
            if (!c) continue;
            addChildComponent(*c);
            face.controls.push_back(std::move(c));
            face.cells.push_back(cells);
        }
        faces_.push_back(std::move(face));
    }
    Look::setTextLook(none_, Look::sans(11.0f), Look::muted, juce::Justification::centredLeft);
    addChildComponent(none_);
    refresh();
}

int DetailsView::rows(const Blocks::Block& b, int face) {
    int n = 0;
    for (const Controls::KnobRow& row : Blocks::kindOf(b).details[face].rows) n += row.id != nullptr ? 1 : 0;
    return n;
}

void DetailsView::refresh() {
    const int now = Controls::faceOf(state_, typeId_, (int)faces_.size());
    if (now != face_) show(now);
}

void DetailsView::show(int face) {
    face_ = face;
    for (size_t f = 0; f < faces_.size(); ++f)
        for (const auto& c : faces_[f].controls) c->setVisible((int)f == face);
    const bool empty = faces_[(size_t)face].controls.empty();
    auto* type = typeId_.isEmpty() ? nullptr : state_.getParameter(typeId_);
    const juce::String name = type != nullptr ? type->getCurrentValueAsText() : juce::String("this block");
    none_.setText("Every setting of " + name + " is on its panel above", juce::dontSendNotification);
    none_.setVisible(empty);
    resized();
}

// The face's controls across in rows, a knob a cell, a box or a switch two
void DetailsView::resized() {
    const auto r = getLocalBounds();
    none_.setBounds(r.withHeight(kSwitchH));
    if (face_ < 0) return;
    const Face& f = faces_[(size_t)face_];
    const int rowH = Knob::heightFor(kDial) + kGap;
    int x = r.getX(), y = r.getY();
    for (size_t i = 0; i < f.controls.size(); ++i) {
        const int w = f.cells[i] * kCell + (f.cells[i] - 1) * kGap;
        if (x + w > r.getRight()) {
            x = r.getX();
            y += rowH;
        }
        const bool knob = f.cells[i] == 1;
        const int h = knob ? Knob::heightFor(kDial)
                           : dynamic_cast<Field*>(f.controls[i].get()) != nullptr ? Field::kHeight : kSwitchH;
        f.controls[i]->setBounds(x, y, w, h);
        x += w + kGap;
    }
}
