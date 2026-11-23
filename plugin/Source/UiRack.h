#pragma once
// D's rack (research/UI_DESIGN.md, "The rack"; UI step 2): the Time Trip column, an OSC panel for each OSC, and a column
// of a COSM panel for each COSM over AMP.  A click on a panel's name selects its block, as the path's does, and the
// selected block's panel has the accent's border.  Its panels are a mode's (v453): the V-Synth's, or the VC-2's for a
// zone (UiVc2Rack.h), laid out the same.
#include "PluginProcessor.h"
#include "UiBlocks.h"
#include "UiCosmPanel.h"
#include "UiOscPanel.h"
#include "UiTripColumn.h"
#include <array>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

// AMP: the patch TVA's level, pan and sends, a panel of one face under the TVA's switch; a mode's knobs (an ID and a name)
class AmpPanel : public FacedPanel {
public:
    using Knobs = std::vector<std::pair<juce::String, juce::String>>;
    explicit AmpPanel(juce::AudioProcessorValueTreeState& state);   // the V-Synth's
    AmpPanel(juce::AudioProcessorValueTreeState& state, const juce::String& switchId, const Knobs& knobs);
    void resized() override;

private:
    void paintDisplay(juce::Graphics&) const override {}
};

class Rack : public juce::Component {
public:
    // What a mode's rack holds, made for it: the Time Trip column, a panel for each OSC and each COSM, and AMP
    struct Panels {
        std::unique_ptr<TripColumn> trip;
        std::array<std::unique_ptr<FacedPanel>, Blocks::kOscs> oscs;
        std::array<std::unique_ptr<FacedPanel>, Blocks::kCosms> cosms;
        std::unique_ptr<FacedPanel> amp;
    };
    explicit Rack(VSynthEmuProcessor& p);   // the V-Synth's
    explicit Rack(Panels panels);
    std::function<void(int block)> onSelect;   // an index into Blocks::kPath
    void setSelected(int block);
    void refresh();                            // each panel's face, wave and display, if they have moved
    void resized() override;

private:
    Panels panels_;
    FacedPanel* panelOf(const Blocks::Block& b);
    void add(FacedPanel& panel, Blocks::Kind kind, int index);   // shown, a click on its name selecting its block
};
