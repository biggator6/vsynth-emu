#pragma once
// What reaches a block (research/UI_DESIGN.md, "The blocks and the lower panel"; UI step 3): a row for each depth by which
// its LFO reaches it (an amber dot) and one for each Matrix route whose destination lands on it (a cyan dot), each
// amount a bar to drag in place.  The Matrix's rows are made again when a route's source or destination moves; a line
// says when none lands on it, and how many more there are than the rows hold, with where routes are made.
#include "PluginProcessor.h"
#include "UiBlockKinds.h"
#include "UiFields.h"
#include <memory>
#include <vector>

class RoutesView : public juce::Component {
public:
    RoutesView(Controls::State& state, const Blocks::Block& b);
    static constexpr int kMatrixControls = 8;   // the XT's Matrix Controls (IDs 0x14d…0x174), two destinations each
    void refresh();                             // the Matrix's rows made again, if a source or destination has moved
    int depths() const { return (int)depths_.size(); }
    int routes() const { return (int)matrix_.size(); }
    Bar* route(int i) const { return matrix_[(size_t)i].get(); }
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    Controls::State& state_;
    Blocks::Block block_;
    juce::Label title_, none_, more_;
    std::vector<std::unique_ptr<Bar>> depths_, matrix_;
    juce::String made_;   // the Matrix's sources and destinations its rows were made from
    void makeRoutes();
};
