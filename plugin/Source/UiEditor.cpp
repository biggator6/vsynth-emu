// D, the redesigned editor (UiEditor.h; UI steps 1 and 2)
#include "UiEditor.h"
#include "UiBlocks.h"
#include "UiParts.h"

namespace {
constexpr int kRefreshHz = 4;
// What a region not built yet says, and the step that builds it
struct NotYet {
    juce::Rectangle<int> r;
    const char* says;
};
}

SynthEditor::SynthEditor(VSynthEmuProcessor& p)
    : juce::AudioProcessorEditor(&p), header_(p), path_(p.apvts), rack_(p), footer_(p) {
    setLookAndFeel(&feel_);
    pageBar_.onPage = [this](int page) { showPage(page); };
    path_.onSelect = [this](int block) { select(block); };
    path_.onEffect = [this](int) { showPage(Blocks::kFxPage); };
    rack_.onSelect = [this](int block) { select(block); };
    const std::initializer_list<juce::Component*> parts = { &header_, &pageBar_, &path_, &rack_, &footer_ };
    for (juce::Component* c : parts) drawing_.addAndMakeVisible(c);
    drawing_.setInterceptsMouseClicks(false, true);
    addAndMakeVisible(drawing_);
    int y = 0;
    for (auto [part, h] : { std::pair<juce::Component*, int> { &header_, Look::headerH }, { &pageBar_, Look::pageBarH },
                            { &path_, Look::pathH }, { &rack_, Look::rackH } }) {
        part->setBounds(0, y, Look::width, h);
        y += h;
    }
    footer_.setBounds(0, Look::height - Look::footerH, Look::width, Look::footerH);
    drawing_.setBounds(0, 0, Look::width, Look::height);
    setResizable(true, true);
    setResizeLimits(juce::roundToInt(Look::width * Look::minScale), juce::roundToInt(Look::height * Look::minScale),
                    juce::roundToInt(Look::width * Look::maxScale), juce::roundToInt(Look::height * Look::maxScale));
    getConstrainer()->setFixedAspectRatio((double)Look::width / (double)Look::height);
    setSize(Look::width, Look::height);
    startTimerHz(kRefreshHz);
}

SynthEditor::~SynthEditor() {
    stopTimer();
    setLookAndFeel(nullptr);
}

void SynthEditor::showPage(int page) {
    pageBar_.setPage(page);
    const bool synth = pageBar_.page() == Blocks::kSynthPage;
    path_.setVisible(synth);
    rack_.setVisible(synth);
    repaint();
}

void SynthEditor::select(int block) {
    path_.setSelected(block);
    rack_.setSelected(block);
}

void SynthEditor::resized() {
    drawing_.setTransform(juce::AffineTransform::scale(scale()));
}

// The ground, and what each region not built yet says (the lower panel, step 3; the pages, step 4)
void SynthEditor::paint(juce::Graphics& g) {
    g.fillAll(Look::ground);
    g.addTransform(juce::AffineTransform::scale(scale()));
    const int top = Look::headerH + Look::pageBarH;
    const int lowerTop = top + Look::pathH + Look::rackH, below = Look::height - Look::footerH;
    const bool synth = pageBar_.page() == Blocks::kSynthPage;
    const NotYet parts[] = {
        { { 0, lowerTop, Look::width, below - lowerTop }, synth ? "The lower panel: UI step 3" : "" },
        { { 0, top, Look::width, below - top }, synth ? "" : "This page: UI step 4" },
    };
    for (const NotYet& n : parts)
        Ui::drawText(g, n.says, n.r, { Look::sans(12.0f), Look::muted, juce::Justification::centred });
    Ui::fill(g, { 0, lowerTop, Look::width, 1 }, synth ? Look::divider : Look::ground);
}

// What the header, the path, the rack and the footer show, each repainting only what moved
void SynthEditor::timerCallback() {
    header_.refresh();
    path_.refresh();
    rack_.refresh();
    footer_.refresh();
}
