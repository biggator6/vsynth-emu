#pragma once
// Controls made from parameters' IDs, each as its parameter is (a choice a box under its name, a switch a switch, any
// other a knob), laid across in rows of cells: a knob takes a cell, a box or a switch two (UI step 4; DETAILS' faces
// from step 3, and the pages' cards).
#include "UiFields.h"
#include <memory>
#include <utility>
#include <vector>

class ControlGrid : public juce::Component {
public:
    using Rows = std::vector<std::pair<juce::String, juce::String>>;   // each control's parameter ID and name
    ControlGrid(Controls::State& state, const Rows& rows);

    static constexpr int kDial = 30, kCell = 64, kGap = 8;
    int made() const { return (int)controls_.size(); }   // fewer than its rows: a row named no parameter
    int heightFor(int width) const;                       // the height its rows take across a width
    void resized() override;

private:
    Controls::Attached attached_;
    std::vector<std::unique_ptr<juce::Component>> controls_;
    std::vector<int> cells_, heights_;   // each control's cells across and its height
    template <class Place>
    int flow(int width, Place&& place) const;
};
