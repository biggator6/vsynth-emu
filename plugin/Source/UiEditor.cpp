// D, the redesigned editor (UiEditor.h; UI steps 1 to 3)
#include "UiEditor.h"
#include "UiBlocks.h"
#include "UiParts.h"

namespace {
constexpr int kRefreshHz = 4;
}

SynthEditor::SynthEditor(VSynthEmuProcessor& p)
    : juce::AudioProcessorEditor(&p), header_(p), path_(p.apvts), rack_(p), lower_(p.apvts), footer_(p) {
    setLookAndFeel(&feel_);
    pageBar_.onChoose = [this](int page) { showPage(page); };
    path_.onSelect = [this](int block) { select(block); };
    path_.onEffect = [this](int) { showPage(Blocks::kFxPage); };
    rack_.onSelect = [this](int block) { select(block); };
    lower_.onSelect = [this](int block) { select(block); };
    const std::initializer_list<juce::Component*> parts = { &header_, &pageBar_, &path_, &rack_, &lower_, &footer_ };
    for (juce::Component* c : parts) drawing_.addAndMakeVisible(c);
    drawing_.setInterceptsMouseClicks(false, true);
    addAndMakeVisible(drawing_);
    int y = 0;
    for (auto [part, h] : { std::pair<juce::Component*, int> { &header_, Look::headerH }, { &pageBar_, Look::pageBarH },
                            { &path_, Look::pathH }, { &rack_, Look::rackH }, { &lower_, Look::lowerH } }) {
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
    select(Blocks::kFirstSelected);
    startTimerHz(kRefreshHz);
}

SynthEditor::~SynthEditor() {
    stopTimer();
    setLookAndFeel(nullptr);
}

void SynthEditor::showPage(int page) {
    pageBar_.choose(page);
    const bool synth = pageBar_.chosen() == Blocks::kSynthPage;
    for (juce::Component* c : std::initializer_list<juce::Component*> { &path_, &rack_, &lower_ }) c->setVisible(synth);
    repaint();
}

void SynthEditor::select(int block) {
    path_.setSelected(block);
    rack_.setSelected(block);
    lower_.setSelected(block);
}

void SynthEditor::resized() {
    drawing_.setTransform(juce::AffineTransform::scale(scale()));
}

// The ground, and what a page not built yet says (step 4)
void SynthEditor::paint(juce::Graphics& g) {
    g.fillAll(Look::ground);
    if (pageBar_.chosen() == Blocks::kSynthPage) return;
    g.addTransform(juce::AffineTransform::scale(scale()));
    const int top = Look::headerH + Look::pageBarH, below = Look::height - Look::footerH;
    Ui::drawText(g, "This page: UI step 4", { 0, top, Look::width, below - top },
                 { Look::sans(12.0f), Look::muted, juce::Justification::centred });
}

// What the header, the path, the rack, the lower panel and the footer show, each repainting only what moved
void SynthEditor::timerCallback() {
    header_.refresh();
    path_.refresh();
    rack_.refresh();
    lower_.refresh();
    footer_.refresh();
}
