#pragma once
// D's rack (research/UI_DESIGN.md, "The rack"; UI step 2): the Time Trip column, an OSC panel for each OSC, and a column
// of a COSM panel for each COSM over AMP.  A click on a panel's name selects its block, as the path's does, and the
// selected block's panel has the accent's border.
#include "PluginProcessor.h"
#include "UiBlocks.h"
#include "UiCosmPanel.h"
#include "UiOscPanel.h"
#include "UiTripColumn.h"
#include <array>
#include <functional>
#include <memory>

// AMP: the patch TVA's level, pan and sends, a panel of one face under the TVA's switch
class AmpPanel : public FacedPanel {
public:
    explicit AmpPanel(juce::AudioProcessorValueTreeState& state);
    void resized() override;

private:
    void paintDisplay(juce::Graphics&) const override {}
};

class Rack : public juce::Component {
public:
    explicit Rack(VSynthEmuProcessor& p);
    std::function<void(int block)> onSelect;   // an index into Blocks::kPath
    void setSelected(int block);
    void refresh();                            // each panel's face, wave and display, if they have moved
    void resized() override;

private:
    TripColumn trip_;
    std::array<std::unique_ptr<OscPanel>, Blocks::kOscs> oscs_;
    std::array<std::unique_ptr<CosmPanel>, Blocks::kCosms> cosms_;
    AmpPanel amp_;
    FacedPanel* panelOf(const Blocks::Block& b);
};
