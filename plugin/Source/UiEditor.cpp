// D, the redesigned editor (UiEditor.h; UI steps 1 to 4)
#include "UiEditor.h"
#include "FxPanel.h"
#include "UiBlocks.h"
#include "UiPages.h"
#include "UiParts.h"

namespace {
constexpr int kRefreshHz = 4;
}

SynthEditor::SynthEditor(VSynthEmuProcessor& p)
    : juce::AudioProcessorEditor(&p), processor_(p), header_(p), path_(p.apvts), rack_(p), lower_(p.apvts), footer_(p) {
    setLookAndFeel(&feel_);
    pageBar_.onChoose = [this](int page) { showPage(page); };
    pageBar_.keyboardSwitch().onClick = [this] { showKeyboard(pageBar_.keyboardSwitch().getToggleState()); };
    path_.onSelect = [this](int block) { select(block); };
    path_.onEffect = [this](int effect) { showEffect(effect); };
    rack_.onSelect = [this](int block) { select(block); };
    lower_.onSelect = [this](int block) { select(block); };
    const std::initializer_list<juce::Component*> parts = { &header_, &pageBar_, &path_, &rack_, &lower_, &footer_ };
    for (juce::Component* c : parts) drawing_.addAndMakeVisible(c);
    const juce::Rectangle<int> pageArea { 0, Look::headerH + Look::pageBarH, Look::width,
                                          Look::height - Look::footerH - Look::headerH - Look::pageBarH };
    for (size_t page = 0; page < pages_.size(); ++page) {
        pages_[page] = Pages::makePage((int)page, p);
        if (!pages_[page]) continue;
        drawing_.addChildComponent(*pages_[page]);
        pages_[page]->setBounds(pageArea);
    }
    drawing_.setInterceptsMouseClicks(false, true);
    addAndMakeVisible(drawing_);
    int y = 0;
    for (auto [part, h] : { std::pair<juce::Component*, int> { &header_, Look::headerH }, { &pageBar_, Look::pageBarH },
                            { &path_, Look::pathH }, { &rack_, Look::rackH }, { &lower_, Look::lowerH } }) {
        part->setBounds(0, y, Look::width, h);
        y += h;
    }
    footer_.setBounds(0, Look::height - Look::footerH, Look::width, Look::footerH);
    setResizable(true, true);
    showKeyboard(p.apvts.state.getProperty(kKeyboardShown, true));
    select(Blocks::kFirstSelected);
    startTimerHz(kRefreshHz);
}

// The drawing as tall as what shows, the window's limits and aspect by it, and the window resized at its scale
void SynthEditor::showKeyboard(bool shown) {
    const float s = getWidth() > 0 ? scale() : 1.0f;
    const int h = Look::height - (shown ? 0 : Look::footerH);
    footer_.setVisible(shown);
    pageBar_.keyboardSwitch().setToggleState(shown, juce::dontSendNotification);
    processor_.apvts.state.setProperty(kKeyboardShown, shown, nullptr);
    drawing_.setBounds(0, 0, Look::width, h);
    setResizeLimits(juce::roundToInt(Look::width * Look::minScale), juce::roundToInt((float)h * Look::minScale),
                    juce::roundToInt(Look::width * Look::maxScale), juce::roundToInt((float)h * Look::maxScale));
    getConstrainer()->setFixedAspectRatio((double)Look::width / (double)h);
    setSize(juce::roundToInt(Look::width * s), juce::roundToInt((float)h * s));
}

SynthEditor::~SynthEditor() {
    stopTimer();
    setLookAndFeel(nullptr);
}

// A page shown, the others hidden (a hidden page draws nothing): SYNTH's the path, the rack and the lower panel
void SynthEditor::showPage(int page) {
    pageBar_.choose(page);
    const bool synth = pageBar_.chosen() == Blocks::kSynthPage;
    for (juce::Component* c : std::initializer_list<juce::Component*> { &path_, &rack_, &lower_ }) c->setVisible(synth);
    for (size_t i = 0; i < pages_.size(); ++i)
        if (pages_[i]) pages_[i]->setVisible((int)i == pageBar_.chosen());
}

// An effect on the path clicked: the FX page, at that effect's own page (0 MFX, 1 Chorus, 2 Reverb)
void SynthEditor::showEffect(int effect) {
    showPage(Blocks::kFxPage);
    if (auto* fx = dynamic_cast<FxPanel*>(pages_[(size_t)Blocks::kFxPage].get())) fx->showPage(effect);
}

void SynthEditor::select(int block) {
    path_.setSelected(block);
    rack_.setSelected(block);
    lower_.setSelected(block);
}

void SynthEditor::resized() {
    drawing_.setTransform(juce::AffineTransform::scale(scale()));
}

void SynthEditor::paint(juce::Graphics& g) {
    g.fillAll(Look::ground);
}

// What the header, the path, the rack, the lower panel and the footer show, each repainting only what moved
void SynthEditor::timerCallback() {
    header_.refresh();
    path_.refresh();
    rack_.refresh();
    lower_.refresh();
    footer_.refresh();
}
