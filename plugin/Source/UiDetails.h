#pragma once
// A block's DETAILS (research/UI_DESIGN.md, "The blocks and the lower panel"; UI step 3): the face of its kind's table
// (UiDetailTables.cpp) its Type shows, a ControlGrid a face; a face with no rows says its settings are all on its panel.
#include "UiBlockKinds.h"
#include "UiControlGrid.h"
#include <memory>
#include <vector>

class DetailsView : public juce::Component {
public:
    DetailsView(Controls::State& state, const Blocks::Block& b);
    void refresh();                                       // its face, if its Type has moved
    int face() const { return face_; }
    int faces() const { return (int)faces_.size(); }
    int made(int face) const { return faces_[(size_t)face]->made(); }   // the controls made for a face
    static int rows(const Blocks::Block& b, int face);    // the rows its kind's table gives a face
    void resized() override;

private:
    Controls::State& state_;
    juce::String typeId_;
    std::vector<std::unique_ptr<ControlGrid>> faces_;
    juce::Label none_;
    int face_ = -1;
    void show(int face);
};
