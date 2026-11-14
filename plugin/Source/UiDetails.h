#pragma once
// A block's DETAILS (research/UI_DESIGN.md, "The blocks and the lower panel"; UI step 3): the face of its kind's table
// (UiDetailTables.cpp) its Type shows, each row a control as its parameter is (a choice a box, a switch a switch, any
// other a knob), laid across in rows; a face with no rows says its settings are all on its panel.
#include "UiBlockKinds.h"
#include "UiFields.h"
#include <memory>
#include <vector>

class DetailsView : public juce::Component {
public:
    DetailsView(Controls::State& state, const Blocks::Block& b);
    void refresh();                                       // its face, if its Type has moved
    int face() const { return face_; }
    int faces() const { return (int)faces_.size(); }
    int made(int face) const { return (int)faces_[(size_t)face].controls.size(); }   // the controls made for a face
    static int rows(const Blocks::Block& b, int face);    // the rows its kind's table gives a face
    void resized() override;

private:
    struct Face {
        std::vector<std::unique_ptr<juce::Component>> controls;
        std::vector<int> cells;   // how many cells across each takes
    };
    Controls::State& state_;
    Controls::Attached attached_;
    juce::String typeId_;
    std::vector<Face> faces_;
    juce::Label none_;
    int face_ = -1;
    void show(int face);
};
