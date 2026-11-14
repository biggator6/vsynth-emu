// What reaches a block (UiRoutes.h; UI step 3)
#include "UiRoutes.h"
#include "UiLook.h"
#include "UiParts.h"

namespace {

constexpr int kTitleH = 22, kGap = 4, kDotW = 14;
constexpr float kDot = 6.0f;
const juce::String kArrow = juce::String::fromUTF8(" \xe2\x86\x92 ");   // " → " between a source and where it lands

int choiceOf(Controls::State& s, const juce::String& id) {
    const auto* v = s.getRawParameterValue(id);
    return v != nullptr ? juce::roundToInt(v->load()) : 0;
}

}

RoutesView::RoutesView(Controls::State& state, const Blocks::Block& b)
    : state_(state), block_(b) {
    Look::panelName(title_, "WHAT REACHES " + Blocks::blockName(b));
    Look::setTextLook(none_, Look::sans(11.0f), Look::muted, juce::Justification::centredLeft);
    Look::setTextLook(more_, Look::sans(11.0f), Look::muted, juce::Justification::centredLeft);
    none_.setText("No Matrix route lands on " + Blocks::blockName(b) + ": the MODULATION page makes them",
                  juce::dontSendNotification);
    addAndMakeVisible(title_);
    addChildComponent(none_);
    addChildComponent(more_);
    for (const Controls::KnobRow& d : Blocks::kindOf(b).depths) {
        const juce::String id = d.id != nullptr ? Blocks::blockParam(b, d.id) : juce::String();
        if (id.isEmpty() || state.getParameter(id) == nullptr) continue;
        depths_.push_back(std::make_unique<Bar>(state, id, Blocks::lfoNameOf(b) + kArrow + d.name, Look::accent));
        addAndMakeVisible(*depths_.back());
    }
    refresh();
}

void RoutesView::makeRoutes() {
    for (const auto& bar : matrix_) removeChildComponent(bar.get());
    matrix_.clear();
    const juce::StringArray sources = VSynthEmuProcessor::matrixSourceNames();
    const juce::StringArray dests = VSynthEmuProcessor::matrixDestNames();
    for (int control = 0; control < Matrix::kControls; ++control) {
        const juce::String source = sources[choiceOf(state_, Matrix::sourceId(control))];
        for (int dest = 0; dest < Matrix::kDests; ++dest) {
            const juce::String to = dests[choiceOf(state_, Matrix::destId(control, dest))];
            if (!Blocks::reaches(block_, to)) continue;
            const juce::String sens = Matrix::sensId(control, dest);
            matrix_.push_back(std::make_unique<Bar>(state_, sens, source + kArrow + to, Look::mod));
            addAndMakeVisible(*matrix_.back());
        }
    }
}

void RoutesView::refresh() {
    juce::String now;
    for (int control = 0; control < Matrix::kControls; ++control) {
        now << choiceOf(state_, Matrix::sourceId(control));
        for (int dest = 0; dest < Matrix::kDests; ++dest) now << ',' << choiceOf(state_, Matrix::destId(control, dest));
        now << '|';
    }
    if (now == made_) return;
    made_ = now;
    makeRoutes();
    resized();
    repaint();
}

// The title, then a row for each depth and each route, as many as fit, then the line saying none lands here, or how
// many more there are
void RoutesView::resized() {
    auto r = getLocalBounds();
    title_.setBounds(r.removeFromTop(kTitleH));
    const int fits = juce::jmax(1, (r.getHeight() + kGap) / (Bar::kRowHeight + kGap) - 1);
    std::vector<Bar*> rows;
    for (const auto* list : { &depths_, &matrix_ })
        for (const auto& bar : *list) rows.push_back(bar.get());
    const int shown = juce::jmin(fits, (int)rows.size());
    const int nameW = (r.getWidth() - kDotW) * 3 / 5;
    for (int i = 0; i < (int)rows.size(); ++i) {
        rows[(size_t)i]->setVisible(i < shown);
        if (i >= shown) continue;
        rows[(size_t)i]->setBounds(r.removeFromTop(Bar::kRowHeight).withTrimmedLeft(kDotW));
        rows[(size_t)i]->setNameWidth(nameW);
        r.removeFromTop(kGap);
    }
    const int hidden = (int)rows.size() - shown;
    more_.setText("+" + juce::String(hidden) + " more: the MODULATION page shows them all", juce::dontSendNotification);
    more_.setVisible(hidden > 0);
    none_.setVisible(matrix_.empty() && hidden == 0);
    const auto line = r.removeFromTop(Bar::kRowHeight).withTrimmedLeft(kDotW);
    more_.setBounds(line);
    none_.setBounds(line);
}

// A dot before each row: the accent for its LFO's depth, the modulation's colour for a Matrix route
void RoutesView::paint(juce::Graphics& g) {
    for (const auto* list : { &depths_, &matrix_ })
        for (const auto& bar : *list)
            if (bar->isVisible())
                Ui::dot(g, { kDot, (float)bar->getBounds().getCentreY() }, kDot, list == &depths_ ? Look::accent
                                                                                                 : Look::mod);
}
