// The Matrix on the MODULATION page (UiMatrix.h; UI step 4)
#include "UiMatrix.h"
#include "UiLook.h"
#include <array>

namespace {

constexpr int kHeadH = 16, kRowGap = 4, kColumnGap = 6;
// The columns across, in their order: the number, Source, then each destination and its Sens; 0 shares what is left
constexpr std::array<int, 6> kWidths { 16, 84, 0, 64, 0, 64 };
const char* const kHeads[] = { "", "Source", "Destination 1", "Sens 1", "Destination 2", "Sens 2" };

// Each column's place across r, its width as kWidths gives it
std::array<juce::Rectangle<int>, kWidths.size()> columnsOf(juce::Rectangle<int> r) {
    int fixed = 0, shares = 0;
    for (int w : kWidths) {
        fixed += w;
        shares += w == 0 ? 1 : 0;
    }
    const int gaps = (int)(kWidths.size() - 1) * kColumnGap;
    const int share = shares > 0 ? (r.getWidth() - fixed - gaps) / shares : 0;
    std::array<juce::Rectangle<int>, kWidths.size()> out;
    for (size_t i = 0; i < kWidths.size(); ++i) {
        out[i] = r.removeFromLeft(kWidths[i] > 0 ? kWidths[i] : share);
        r.removeFromLeft(kColumnGap);
    }
    return out;
}

}

MatrixRow::MatrixRow(Controls::State& state, int control) {
    Look::setTextLook(number_, Look::mono(11.0f), Look::label);
    number_.setText(juce::String(control + 1), juce::dontSendNotification);
    addAndMakeVisible(number_);
    made_ += Controls::box(source_, state, Matrix::sourceId(control), attached_) ? 1 : 0;
    addAndMakeVisible(source_);
    for (int d = 0; d < Matrix::kDests; ++d) {
        made_ += Controls::box(dests_[(size_t)d], state, Matrix::destId(control, d), attached_) ? 1 : 0;
        addAndMakeVisible(dests_[(size_t)d]);
        const juce::String sens = Matrix::sensId(control, d);
        if (state.getParameter(sens) == nullptr) continue;
        sens_[(size_t)d] = std::make_unique<Bar>(state, sens, juce::String(), Look::mod);
        addAndMakeVisible(*sens_[(size_t)d]);
        ++made_;
    }
}

// The number and Source, then each destination's box and Sens in the columns after them
void MatrixRow::resized() {
    const auto c = columnsOf(getLocalBounds());
    number_.setBounds(c[0]);
    source_.setBounds(c[1]);
    for (size_t d = 0; d < dests_.size(); ++d) {
        dests_[d].setBounds(c[2 + 2 * d]);
        if (sens_[d]) sens_[d]->setBounds(c[3 + 2 * d]);
    }
}

MatrixView::MatrixView(Controls::State& state) {
    for (const char* name : kHeads) {
        auto head = std::make_unique<juce::Label>();
        Look::fieldName(*head, name);
        addAndMakeVisible(*head);
        heads_.push_back(std::move(head));
    }
    for (int control = 0; control < Matrix::kControls; ++control) {
        rows_.push_back(std::make_unique<MatrixRow>(state, control));
        addAndMakeVisible(*rows_.back());
    }
}

int MatrixView::height() {
    return kHeadH + Matrix::kControls * (MatrixRow::kH + kRowGap);
}

void MatrixView::resized() {
    auto r = getLocalBounds();
    const auto c = columnsOf(r.removeFromTop(kHeadH));
    for (size_t i = 0; i < heads_.size(); ++i) heads_[i]->setBounds(c[i]);
    for (const auto& row : rows_) {
        r.removeFromTop(kRowGap);
        row->setBounds(r.removeFromTop(MatrixRow::kH));
    }
}
