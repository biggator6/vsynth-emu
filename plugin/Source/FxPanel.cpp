#include "FxPanel.h"
#include "PluginProcessor.h"

namespace {
const juce::Colour kBack(0xFF15152E), kLine(0xFF333355), kTick(0xFF00BFA5), kBox(0xFF0D0D1A);
constexpr int kRowH = 26, kParamH = 24, kLabelW = 64, kNameW = 170, kValueW = 92, kScrollBarW = 12;
constexpr int kOutputGain = 0x76;

void styleLabel(juce::Label& l) {
    l.setFont(juce::Font(juce::FontOptions(11.0f)));
    l.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    l.setJustificationType(juce::Justification::centredLeft);
    l.setMinimumHorizontalScale(0.7f);
}

void styleSlider(juce::Slider& s, int textBoxW) {
    s.setSliderStyle(juce::Slider::LinearHorizontal);
    s.setColour(juce::Slider::textBoxTextColourId, juce::Colours::lightgrey);
    s.setColour(juce::Slider::textBoxBackgroundColourId, kBox);
    s.setColour(juce::Slider::textBoxOutlineColourId, kLine);
    s.setColour(juce::Slider::trackColourId, kTick);
    s.setTextBoxStyle(juce::Slider::TextBoxRight, false, textBoxW, 20);
}

void styleToggle(juce::ToggleButton& b) {
    b.setColour(juce::ToggleButton::textColourId, juce::Colours::lightgrey);
    b.setColour(juce::ToggleButton::tickColourId, kTick);
}
}

FxPanel::Level& FxPanel::addLevel(std::vector<std::unique_ptr<Level>>& to, const char* text, const char* parameter) {
    auto level = std::make_unique<Level>();
    level->label.setText(text, juce::dontSendNotification);
    styleLabel(level->label);
    styleSlider(level->slider, 34);
    addAndMakeVisible(level->label);
    addAndMakeVisible(level->slider);
    level->att = std::make_unique<SliderAtt>(processor_.apvts, parameter, level->slider);
    to.push_back(std::move(level));
    return *to.back();
}

void FxPanel::addBlock(Block& b, const char* name, const char* onParam, const char* typeParam, int types) {
    b.on.setButtonText(name);
    styleToggle(b.on);
    addAndMakeVisible(b.on);
    b.onAtt = std::make_unique<ButtonAtt>(processor_.apvts, onParam, b.on);
    for (int t = 0; t < types; ++t) b.type.addItem("Type " + juce::String(t), t + 1);   // their names once the ROM's here
    addAndMakeVisible(b.type);
    b.typeAtt = std::make_unique<ComboAtt>(processor_.apvts, typeParam, b.type);
}

FxPanel::FxPanel(VSynthEmuProcessor& p) : processor_(p) {
    setOpaque(true);
    styleLabel(status_);
    addAndMakeVisible(status_);

    addBlock(mfx_, "MFX", "fxMfxOn", "fxMfxType", 42);
    addLevel(mfx_.levels, "Master", "fxMfxMaster");
    addLevel(mfx_.levels, "To Chorus", "fxMfxCho");
    addLevel(mfx_.levels, "To Reverb", "fxMfxRev");
    addBlock(chorus_, "Chorus", "fxChoOn", "fxChoType", 9);
    addLevel(chorus_.levels, "Master", "fxChoMaster");
    addLevel(chorus_.levels, "To Reverb", "fxChoRev");
    addBlock(reverb_, "Reverb", "fxRevOn", "fxRevType", 11);
    addLevel(reverb_.levels, "Master", "fxRevMaster");

    addLevel(tone_, "Chorus Send", "tvaChoSend");
    addLevel(tone_, "Reverb Send", "tvaRevSend");
    addLevel(system_, "Master", "fxMaster");
    styleSlider(addLevel(system_, "Output Gain", "fxGain").slider, 52);                  // the one level with a unit
    for (auto* l : { &toneLabel_, &systemLabel_, &assignLabel_, &modeLabel_, &pageLabel_ }) { styleLabel(*l); addAndMakeVisible(*l); }
    toneLabel_.setText("The tone", juce::dontSendNotification);
    systemLabel_.setText("System", juce::dontSendNotification);
    assignLabel_.setText("Output Assign", juce::dontSendNotification);
    modeLabel_.setText("Output Mode", juce::dontSendNotification);
    assignBox_.addItemList({ "MULTI", "MAIN", "DIR" }, 1);
    modeBox_.addItemList({ "Mix", "Parallel" }, 1);
    addAndMakeVisible(assignBox_);
    addAndMakeVisible(modeBox_);
    assignAtt_ = std::make_unique<ComboAtt>(processor_.apvts, "outputAssign", assignBox_);
    modeAtt_ = std::make_unique<ComboAtt>(processor_.apvts, "outputMode", modeBox_);

    pageBox_.addItemList({ "MFX", "Chorus", "Reverb", "System EQ" }, 1);
    pageBox_.setSelectedId(1, juce::dontSendNotification);
    pageBox_.onChange = [this] { refresh(); };
    addAndMakeVisible(pageBox_);
    view_.setViewedComponent(&grid_, false);
    view_.setScrollBarsShown(true, false);
    addAndMakeVisible(view_);
}

FxPanel::~FxPanel() { view_.setViewedComponent(nullptr, false); }

void FxPanel::showPage(int page) {
    pageBox_.setSelectedId(page + 1, juce::dontSendNotification);
    refresh();
}

void FxPanel::paint(juce::Graphics& g) {
    g.fillAll(kBack);
    g.setColour(kLine);
    g.drawLine(0.0f, (float)ruleY_, (float)getWidth(), (float)ruleY_, 1.0f);
}

void FxPanel::layoutLevels(juce::Rectangle<int> row, std::vector<std::unique_ptr<Level>>& levels, int across) {
    const int w = row.getWidth() / across;
    for (auto& l : levels) {
        auto cell = row.removeFromLeft(w).reduced(0, 2);
        l->label.setBounds(cell.removeFromLeft(kLabelW));
        l->slider.setBounds(cell.withTrimmedRight(8));
    }
}

void FxPanel::resized() {
    auto r = getLocalBounds().reduced(10, 6);
    status_.setBounds(r.removeFromTop(18));
    r.removeFromTop(2);
    const auto block = [&](Block& b) {
        auto head = r.removeFromTop(kRowH);
        b.on.setBounds(head.removeFromLeft(kLabelW + 8));
        b.type.setBounds(head.reduced(0, 2));
        layoutLevels(r.removeFromTop(kRowH), b.levels, 3);
        r.removeFromTop(4);
    };
    block(mfx_);
    block(chorus_);
    block(reverb_);
    {
        auto row = r.removeFromTop(kRowH);
        toneLabel_.setBounds(row.removeFromLeft(kLabelW));
        layoutLevels(row, tone_, 2);
        row = r.removeFromTop(kRowH);
        row.removeFromLeft(kLabelW);
        auto left = row.removeFromLeft(row.getWidth() / 2);
        assignLabel_.setBounds(left.removeFromLeft(kLabelW + 10));
        assignBox_.setBounds(left.withTrimmedRight(8).reduced(0, 2));
        modeLabel_.setBounds(row.removeFromLeft(kLabelW + 10));
        modeBox_.setBounds(row.withTrimmedRight(8).reduced(0, 2));
        row = r.removeFromTop(kRowH);
        systemLabel_.setBounds(row.removeFromLeft(kLabelW));
        layoutLevels(row, system_, 2);
    }
    r.removeFromTop(6);
    ruleY_ = r.getY() + 6;
    r.removeFromTop(12);
    auto page = r.removeFromTop(kRowH);
    pageBox_.setBounds(page.removeFromLeft(120).reduced(0, 2));
    page.removeFromLeft(10);
    pageLabel_.setBounds(page);
    r.removeFromTop(4);
    view_.setBounds(r);
    grid_.setSize(view_.getWidth() - kScrollBarW, grid_.getHeight());
    int y = 0;
    for (auto& row : rows_) {
        juce::Rectangle<int> b(0, y, grid_.getWidth(), kParamH);
        row->name.setBounds(b.removeFromLeft(kNameW));
        row->value.setBounds(b.withTrimmedRight(4));
        y += kParamH;
    }
}

void FxPanel::visibilityChanged() {
    if (isVisible()) { refresh(); startTimerHz(5); }
    else stopTimer();
}

void FxPanel::timerCallback() { refresh(); }

// What's on a page follows its block's type, and the values follow the processor (a host's saved state), but for one being dragged.
void FxPanel::refresh() {
    const bool wanted = processor_.apvts.getRawParameterValue("fxOn")->load() >= 0.5f;
    status_.setText(!wanted ? "Effects off: the sound goes round the chip. Settings here wait for it."
                    : processor_.effectsRunning() ? "Effects on: the sound goes through the chip."
                    : "Effects on, not running: they need the ROM.", juce::dontSendNotification);

    const XtFxCatalog& cat = processor_.fxCatalog();
    if (cat.ok && !named_) {
        // A combo's selection counts only while its text equals the item's, so it's read before the names change and put back after.
        const auto name = [](juce::ComboBox& box, const std::vector<std::string>& names) {
            const int selected = box.getSelectedItemIndex();
            for (size_t t = 0; t < names.size(); ++t) box.changeItemText((int)t + 1, names[t]);
            if (selected >= 0) box.setText(box.getItemText(selected), juce::dontSendNotification);
        };
        name(mfx_.type, cat.mfxTypes);
        name(chorus_.type, cat.chorusTypes);
        name(reverb_.type, cat.reverbTypes);
        juce::Slider& gain = system_.back()->slider;
        gain.textFromValueFunction = [values = cat.values](double v) { return juce::String(values->text(kOutputGain, (int)std::lround(v))); };
        gain.updateText();
        named_ = true;
        shownPage_ = -1;
    }

    const int page = pageBox_.getSelectedItemIndex();
    const int type = page == 0 ? mfx_.type.getSelectedItemIndex() : page == 1 ? chorus_.type.getSelectedItemIndex() : page == 2 ? reverb_.type.getSelectedItemIndex() : 0;
    if (page != shownPage_ || type != shownType_) {
        shownPage_ = page; shownType_ = type;
        static const std::vector<XtFxParam> none;
        const std::vector<std::vector<XtFxParam>>* types = page == 0 ? &cat.mfx : page == 1 ? &cat.chorus : page == 2 ? &cat.reverb : nullptr;
        const std::vector<XtFxParam>& params = !cat.ok ? none : page == 3 ? cat.eq : (size_t)type < types->size() ? (*types)[(size_t)type] : none;
        showParams(params);
        const juce::String title = !cat.ok ? juce::String("The ROM has the names and ranges.")
                                 : page == 3 ? juce::String("The system's EQ, after the mix")
                                 : (page == 0 ? mfx_ : page == 1 ? chorus_ : reverb_).type.getText() + (params.empty() ? ": nothing to set" : "");
        pageLabel_.setText(title, juce::dontSendNotification);
    }
    for (auto& row : rows_)
        if (row->id >= 0 && !row->value.isMouseButtonDown(true) && !row->value.hasKeyboardFocus(true))
            row->value.setValue(processor_.fxParam(row->id, row->def), juce::dontSendNotification);
}

void FxPanel::showParams(const std::vector<XtFxParam>& params) {
    while (rows_.size() < params.size()) {
        auto row = std::make_unique<Row>();
        styleLabel(row->name);
        styleSlider(row->value, kValueW);
        grid_.addChildComponent(row->name);
        grid_.addChildComponent(row->value);
        rows_.push_back(std::move(row));
    }
    const std::shared_ptr<XtFxValueText> values = processor_.fxCatalog().values;
    for (size_t i = 0; i < rows_.size(); ++i) {
        Row& row = *rows_[i];
        const bool shown = i < params.size();
        row.value.onValueChange = nullptr;
        row.name.setVisible(shown);
        row.value.setVisible(shown);
        row.id = -1;
        if (!shown) continue;
        const XtFxParam& p = params[i];
        row.id = p.id; row.def = p.def;
        row.name.setText(p.name, juce::dontSendNotification);
        // The text before the range, as setRange() shows it: the same number can read differently on the next parameter.
        row.value.textFromValueFunction = [values, id = p.id](double v) { return juce::String(values->text(id, (int)std::lround(v))); };
        row.value.valueFromTextFunction = [values, &row, id = p.id, low = p.low, high = p.high](const juce::String& typed) { return (double)values->value(id, low, high, typed.toStdString(), (int)std::lround(row.value.getValue())); };
        row.value.setRange(p.low, p.high, 1.0);
        row.value.setDoubleClickReturnValue(true, p.def);
        row.value.onValueChange = [this, &row] { processor_.setFxParam(row.id, (int)std::lround(row.value.getValue())); };
    }
    grid_.setSize(view_.getWidth() - kScrollBarW, (int)params.size() * kParamH);
    resized();
    view_.setViewPosition(0, 0);
}
