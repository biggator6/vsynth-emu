// A block's DETAILS (UiDetails.h; UI step 3; its controls a ControlGrid a face from UI step 4)
#include "UiDetails.h"
#include "UiLook.h"

namespace {
constexpr int kFaintH = 20;
}

DetailsView::DetailsView(Controls::State& state, const Blocks::Block& b)
    : state_(state), typeId_(Blocks::typeIdOf(b)) {
    const Blocks::BlockKind& kind = Blocks::kindOf(b);
    for (int f = 0; f < kind.faces; ++f) {
        ControlGrid::Rows rows;
        for (const Controls::KnobRow& row : kind.details[f].rows) {
            if (row.id == nullptr) break;
            rows.push_back({ Blocks::blockParam(b, row.id), row.name });
        }
        faces_.push_back(std::make_unique<ControlGrid>(state, rows));
        addChildComponent(*faces_.back());
    }
    Look::setTextLook(none_, Look::sans(11.0f), Look::muted, juce::Justification::centredLeft);
    addChildComponent(none_);
    Look::setTextLook(faint_, Look::sans(11.0f), Look::muted, juce::Justification::centredLeft);
    faint_.setText("Faint: rom.4 holds these, and nothing found reads them", juce::dontSendNotification);
    addChildComponent(faint_);
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
    for (size_t f = 0; f < faces_.size(); ++f) faces_[f]->setVisible((int)f == face && faces_[f]->made() > 0);
    auto* type = typeId_.isEmpty() ? nullptr : state_.getParameter(typeId_);
    const juce::String name = type != nullptr ? Controls::currentText(state_, typeId_) : juce::String("this block");
    none_.setText("Every setting of " + name + " is on its panel above", juce::dontSendNotification);
    none_.setVisible(faces_[(size_t)face]->made() == 0);
    faint_.setVisible(faces_[(size_t)face]->dimmed() > 0);
}

// The face's grid, and under it the line saying what is faint when it has any (the VC-2's, d-6)
void DetailsView::resized() {
    none_.setBounds(getLocalBounds().withHeight(24));
    faint_.setBounds(getLocalBounds().removeFromBottom(kFaintH));
    for (const auto& f : faces_) f->setBounds(getLocalBounds().withTrimmedBottom(f->dimmed() > 0 ? kFaintH : 0));
}
