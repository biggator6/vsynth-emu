// D, the redesigned editor (UiEditor.h; UI steps 1 to 4; the VC-2 mode, v453)
#include "UiEditor.h"
#include "FxPanel.h"
#include "UiBlocks.h"
#include "UiPages.h"
#include "UiParts.h"
#include "UiVc2Pages.h"
#include "UiVc2VocoderPage.h"

namespace {
constexpr int kRefreshHz = 4;

// A mode's pages by its maker, each over the page area, hidden until shown; none for SYNTH
using PageMaker = std::unique_ptr<juce::Component> (*)(int page, VSynthEmuProcessor&);
template <size_t N>
void addPages(std::array<std::unique_ptr<juce::Component>, N>& into, PageMaker make, VSynthEmuProcessor& p,
              juce::Component& drawing, juce::Rectangle<int> area) {
    for (size_t page = 0; page < N; ++page) {
        into[page] = make((int)page, p);
        if (!into[page]) continue;
        drawing.addChildComponent(*into[page]);
        into[page]->setBounds(area);
    }
}
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
    header_.onMode = [this](PluginMode mode) { showMode(mode); };
    header_.onMic = [this] { showPage(Blocks::kVc2VocoderPage); };
    const std::initializer_list<juce::Component*> parts = { &header_, &pageBar_, &path_, &rack_, &lower_, &footer_ };
    for (juce::Component* c : parts) drawing_.addAndMakeVisible(c);
    addPages(pages_, Pages::makePage, p, drawing_, pageArea());
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
    showMode(pluginMode(p.apvts.state));
    learnMenus_.listenTo(*this);   // v463: a right-click on any control under it, MIDI learn's menu
    startTimerHz(kRefreshHz);
}

// Everything under the page bar and over the footer
juce::Rectangle<int> SynthEditor::pageArea() const {
    return { 0, Look::headerH + Look::pageBarH, Look::width,
             Look::height - Look::footerH - Look::headerH - Look::pageBarH };
}

// The VC-2's pages, made the first time its mode shows: its SYNTH page and the others by its table (UiVc2Pages.h)
void SynthEditor::makeVc2() {
    vc2Synth_ = std::make_unique<Vc2SynthPage>(processor_);
    vc2Synth_->onEffect = [this](int effect) { showEffect(effect); };
    vc2Synth_->onVocoder = [this] { showPage(Blocks::kVc2VocoderPage); };
    drawing_.addChildComponent(*vc2Synth_);
    vc2Synth_->setBounds(pageArea());
    addPages(vc2Pages_, Vc2Pages::makeVc2Page, processor_, drawing_, pageArea());
}

void SynthEditor::showMode(PluginMode mode) {
    mode_ = mode;
    setPluginMode(processor_.apvts.state, mode);
    if (vc2() && !vc2Synth_) makeVc2();
    header_.setMode(mode);
    pageBar_.showVc2(vc2());
    footer_.setVc2(vc2());
    showPage(Blocks::kSynthPage);
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

juce::Component* SynthEditor::pageView(int page) {
    return vc2() ? vc2Pages_[(size_t)page].get() : pages_[(size_t)page].get();
}

// A page shown, the others hidden (a hidden page draws nothing): SYNTH's the path, the rack and the lower panel; the
// other mode's all hidden
void SynthEditor::showPage(int page) {
    pageBar_.choose(page);
    const bool synth = pageBar_.chosen() == Blocks::kSynthPage && !vc2();
    for (juce::Component* c : std::initializer_list<juce::Component*> { &path_, &rack_, &lower_ }) c->setVisible(synth);
    for (size_t i = 0; i < pages_.size(); ++i)
        if (pages_[i]) pages_[i]->setVisible((int)i == pageBar_.chosen() && !vc2());
    if (!vc2Synth_) return;
    vc2Synth_->setVisible(vc2() && pageBar_.chosen() == Blocks::kSynthPage);
    for (size_t i = 0; i < vc2Pages_.size(); ++i)
        if (vc2Pages_[i]) vc2Pages_[i]->setVisible(vc2() && (int)i == pageBar_.chosen());
    if (auto* vocoder = dynamic_cast<Vc2VocoderPage*>(vc2Pages_[(size_t)Blocks::kVc2VocoderPage].get()))
        if (vocoder->isVisible()) vocoder->refresh();   // its checks as they stand as it shows, not at the next tick
}

// An effect on the path clicked: the FX page, at that effect's own page (0 MFX, 1 Chorus, 2 Reverb); the VC-2's FX page
// is one page of them all
void SynthEditor::showEffect(int effect) {
    if (vc2()) return showPage(Blocks::kVc2FxPage);
    showPage(Blocks::kFxPage);
    if (auto* fx = dynamic_cast<FxPanel*>(pages_[(size_t)Blocks::kFxPage].get())) fx->showPage(effect);
}

void SynthEditor::select(int block) {
    if (vc2() && vc2Synth_) return vc2Synth_->select(block);
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

// What the header, the path, the rack, the lower panel and the footer show, each repainting only what moved; in the
// VC-2 mode its SYNTH page's, and the VOCODER page's checks while it shows
void SynthEditor::timerCallback() {
    header_.refresh();
    footer_.refresh();
    pageBar_.showLine(MidiLearn::learning(processor_));   // v463: which parameter is learning a controller
    if (!vc2()) {
        path_.refresh();
        rack_.refresh();
        lower_.refresh();
        return;
    }
    vc2Synth_->refresh();
    if (auto* vocoder = dynamic_cast<Vc2VocoderPage*>(vc2Pages_[(size_t)Blocks::kVc2VocoderPage].get()))
        if (vocoder->isVisible()) vocoder->refresh();
}
