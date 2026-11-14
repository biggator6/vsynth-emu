#pragma once
// D's lower panel (research/UI_DESIGN.md, "The blocks and the lower panel"; UI step 3): the selected block's name over its
// list (SELECTED, DETAILS, ENVELOPES, LFOs), and the chosen view: SELECTED, the block's envelopes, its LFO and what
// reaches it (MOD, which has none of them, shows its DETAILS there); DETAILS, its other settings; ENVELOPES and LFOs,
// every one at once, a click opening its block.  Each block's views are made once from its kind's row and its index;
// only the shown ones are visible, and only they follow what moves.
#include "UiBlockKinds.h"
#include "UiDetails.h"
#include "UiEnvelope.h"
#include "UiLfo.h"
#include "UiOverviews.h"
#include "UiRoutes.h"
#include "UiTabs.h"
#include <array>
#include <functional>
#include <memory>

// The list: its names down its width, the chosen one filled
class ViewList : public Tabs {
public:
    ViewList();
    std::vector<juce::Rectangle<int>> tabs() const override;

protected:
    void paintTab(juce::Graphics& g, int tab, juce::Rectangle<int> r) const override;
};

class LowerPanel : public juce::Component {
public:
    explicit LowerPanel(Controls::State& state);
    enum class View { selected, details, envelopes, lfos };

    std::function<void(int block)> onSelect;   // a click on an overview's tile: an index into Blocks::kPath
    void setSelected(int block);
    void showView(View v);
    View view() const { return (View)list_.chosen(); }
    void refresh();                            // the shown views' routes and faces, if what they follow has moved
    ViewList& list() { return list_; }

    // A block's views (none where its kind has none), and the overviews
    EnvelopeView* envelopes(int block) { return blocks_[(size_t)block].env.get(); }
    LfoView* lfo(int block) { return blocks_[(size_t)block].lfo.get(); }
    RoutesView* routes(int block) { return blocks_[(size_t)block].routes.get(); }
    DetailsView& details(int block) { return *blocks_[(size_t)block].details; }
    Overview& allEnvelopes() { return allEnvelopes_; }
    Overview& allLfos() { return allLfos_; }

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    // A column of SELECTED: its view and its width across
    struct Column {
        juce::Component* view;
        int width;
    };
    struct BlockViews {
        std::unique_ptr<EnvelopeView> env;
        std::unique_ptr<LfoView> lfo;
        std::unique_ptr<RoutesView> routes;
        std::unique_ptr<DetailsView> details;
        std::array<Column, 3> columns() const;
    };
    ViewList list_;
    juce::Label name_;
    std::array<BlockViews, Blocks::kCount> blocks_;
    EnvelopesOverview allEnvelopes_;
    LfosOverview allLfos_;
    int selected_ = Blocks::kFirstSelected;
    void showNow();
    juce::Rectangle<int> viewArea() const;
};
