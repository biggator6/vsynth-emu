// D's lower panel (UiLowerPanel.h; UI step 3)
#include "UiLowerPanel.h"
#include "UiLook.h"
#include "UiParts.h"

namespace {

constexpr int kListW = 148, kNameH = 24, kEntryH = 28, kInsetY = 10, kSplit = 12;
constexpr int kEnvW = 392, kLfoW = 214, kColumnGap = 16;
const juce::StringArray kViewNames { "SELECTED", "DETAILS", "ENVELOPES", "LFOs" };

}

ViewList::ViewList()
    : Tabs(kViewNames, { Look::sans(11.0f, "Bold", 0.08f), 0, 10 }) {}

std::vector<juce::Rectangle<int>> ViewList::tabs() const {
    std::vector<juce::Rectangle<int>> r;
    for (int i = 0; i < names().size(); ++i) r.push_back({ 0, i * kEntryH, getWidth(), kEntryH - 2 });
    return r;
}

void ViewList::paintTab(juce::Graphics& g, int tab, juce::Rectangle<int> r) const {
    const bool open = tab == chosen();
    if (open) Ui::fillRounded(g, r.toFloat(), Look::divider, 4.0f);
    const Ui::TextStyle look { style_.font, open ? Look::text : Look::label, juce::Justification::centredLeft, false };
    Ui::drawText(g, names()[tab], r.withTrimmedLeft(style_.pad), look);
}

// SELECTED's columns in their order, each with its width (0: what is left); none where the block's kind has none
std::array<LowerPanel::Column, 3> LowerPanel::BlockViews::columns() const {
    const Column envelopes { env.get(), kEnvW };
    const Column shape { lfo.get(), kLfoW };
    const Column reaching { routes.get(), 0 };
    return { envelopes, shape, reaching };
}

LowerPanel::LowerPanel(Controls::State& state)
    : allEnvelopes_(state), allLfos_(state) {
    for (int block = 0; block < Blocks::kCount; ++block) {
        const Blocks::Block& b = Blocks::kPath[(size_t)block];
        BlockViews& v = blocks_[(size_t)block];
        if (Blocks::envelopesOf(b) > 0) v.env = std::make_unique<EnvelopeView>(state, b);
        if (Blocks::kindOf(b).lfo != nullptr) v.lfo = std::make_unique<LfoView>(state, b);
        if (Blocks::kindOf(b).dests.front() != nullptr) v.routes = std::make_unique<RoutesView>(state, b);
        v.details = std::make_unique<DetailsView>(state, b);
        for (const Column& c : v.columns())
            if (c.view != nullptr) addChildComponent(c.view);
        addChildComponent(*v.details);
    }
    const auto pick = [this](int block, int env) {
        if (onSelect) onSelect(block);
        setSelected(block);
        if (auto* e = envelopes(block)) e->showEnvelope(env);
        showView(View::selected);
    };
    allEnvelopes_.onPick = pick;
    allLfos_.onPick = pick;
    addChildComponent(allEnvelopes_);
    addChildComponent(allLfos_);
    list_.onChoose = [this](int) { showNow(); };
    Look::panelName(name_, {});
    name_.setColour(juce::Label::textColourId, Look::accent);
    addAndMakeVisible(list_);
    addAndMakeVisible(name_);
    showNow();
}

void LowerPanel::setSelected(int block) {
    selected_ = juce::jlimit(0, Blocks::kCount - 1, block);
    showNow();
}

void LowerPanel::showView(View v) {
    list_.choose((int)v);
    showNow();
}

// What is shown: the selected block's views for SELECTED or DETAILS, or an overview; everything else hidden
void LowerPanel::showNow() {
    name_.setText(Blocks::blockName(Blocks::kPath[(size_t)selected_]), juce::dontSendNotification);
    const View v = view();
    for (int block = 0; block < Blocks::kCount; ++block) {
        BlockViews& b = blocks_[(size_t)block];
        const bool mine = block == selected_;
        const bool selected = mine && v == View::selected;
        bool any = false;
        for (const Column& c : b.columns()) {
            if (c.view == nullptr) continue;
            c.view->setVisible(selected);
            any = true;
        }
        b.details->setVisible((mine && v == View::details) || (selected && !any));
    }
    allEnvelopes_.setVisible(v == View::envelopes);
    allLfos_.setVisible(v == View::lfos);
    refresh();
    resized();
    repaint();
}

void LowerPanel::refresh() {
    BlockViews& b = blocks_[(size_t)selected_];
    if (b.routes && b.routes->isVisible()) b.routes->refresh();
    if (b.details->isVisible()) b.details->refresh();
}

// The name and the list at the left; the views at the right, SELECTED's columns across as the block has them
void LowerPanel::resized() {
    const auto inside = getLocalBounds().reduced(Look::inset, kInsetY);
    auto left = inside.withWidth(kListW);
    name_.setBounds(left.removeFromTop(kNameH));
    list_.setBounds(left);
    const auto area = inside.withTrimmedLeft(kListW + 2 * kSplit);
    allEnvelopes_.setBounds(area);
    allLfos_.setBounds(area);
    for (BlockViews& b : blocks_) {
        b.details->setBounds(area);
        auto r = area;
        for (const Column& c : b.columns()) {
            if (c.view == nullptr) continue;
            c.view->setBounds(c.width > 0 ? r.removeFromLeft(c.width) : r);
            r.removeFromLeft(kColumnGap);
        }
    }
}

void LowerPanel::paint(juce::Graphics& g) {
    g.fillAll(Look::lower);
    Ui::fill(g, getLocalBounds().removeFromTop(1), Look::divider);
    const int x = Look::inset + kListW + kSplit;
    Ui::fill(g, { x, kInsetY, 1, getHeight() - 2 * kInsetY }, Look::divider);
}
