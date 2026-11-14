// Controls made from parameters' IDs, laid in cells (UiControlGrid.h; UI step 4)
#include "UiControlGrid.h"
#include "UiLook.h"

namespace {

constexpr int kSwitchH = 24;

// A row's control as its parameter is, its cells across and its height: a choice's box and a switch two cells, a knob
// one; none if the ID names no parameter (a table's mistake, which the editor's test reports)
std::unique_ptr<juce::Component> controlFor(Controls::State& s, const juce::String& id, const juce::String& name,
                                            Controls::Attached& attached, int& cells, int& height) {
    auto* p = s.getParameter(id);
    if (p == nullptr) return nullptr;
    cells = 2;
    height = Field::kHeight;
    if (dynamic_cast<juce::AudioParameterChoice*>(p) != nullptr) return std::make_unique<Field>(s, id, name);
    height = kSwitchH;
    if (dynamic_cast<juce::AudioParameterBool*>(p) != nullptr) {
        auto t = std::make_unique<juce::ToggleButton>(name);
        Controls::toggle(*t, s, id, attached);
        return t;
    }
    cells = 1;
    height = Knob::heightFor(ControlGrid::kDial);
    return Controls::knob(s, id, name, ControlGrid::kDial);
}

}

ControlGrid::ControlGrid(Controls::State& state, const Rows& rows) {
    for (const auto& [id, name] : rows) {
        int cells = 1, height = 0;
        auto c = controlFor(state, id, name, attached_, cells, height);
        if (!c) continue;
        addAndMakeVisible(*c);
        controls_.push_back(std::move(c));
        cells_.push_back(cells);
        heights_.push_back(height);
    }
}

// Each control in turn across rows of a width, a row as high as its tallest; place(i, its bounds); the height taken
template <class Place>
int ControlGrid::flow(int width, Place&& place) const {
    int x = 0, y = 0, rowH = 0;
    for (size_t i = 0; i < controls_.size(); ++i) {
        const int w = cells_[i] * kCell + (cells_[i] - 1) * kGap;
        if (x > 0 && x + w > width) {
            x = 0;
            y += rowH + kGap;
            rowH = 0;
        }
        place(i, juce::Rectangle<int>(x, y, w, heights_[i]));
        x += w + kGap;
        rowH = juce::jmax(rowH, heights_[i]);
    }
    return y + rowH;
}

int ControlGrid::heightFor(int width) const {
    return flow(width, [](size_t, juce::Rectangle<int>) {});
}

void ControlGrid::resized() {
    flow(getWidth(), [this](size_t i, juce::Rectangle<int> r) { controls_[i]->setBounds(r); });
}
