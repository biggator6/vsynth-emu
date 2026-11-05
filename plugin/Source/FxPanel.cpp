#include "FxPanel.h"
#include "PluginProcessor.h"

namespace {
constexpr int kParamH = 24, kNameW = 130, kValueW = 92, kScrollBarW = 12, kPageButtonsW = 300, kTypeLabelW = 40, kToggleW = 54, kLeftW = 250;
constexpr int kOutputGain = 0x76;

void styleLabel(juce::Label& l) {
    l.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    l.setColour(juce::Label::textColourId, Ui::label);
    l.setJustificationType(juce::Justification::centredLeft);
    l.setMinimumHorizontalScale(0.7f);
}

void styleSlider(juce::Slider& s, int textBoxW) {
    s.setSliderStyle(juce::Slider::LinearHorizontal);
    s.setColour(juce::Slider::textBoxTextColourId, Ui::text);
    s.setColour(juce::Slider::textBoxBackgroundColourId, Ui::surfaceField);
    s.setColour(juce::Slider::textBoxOutlineColourId, Ui::line);
    s.setColour(juce::Slider::trackColourId, Ui::accent);
    s.setTextBoxStyle(juce::Slider::TextBoxRight, false, textBoxW, 20);
}

void styleToggle(juce::ToggleButton& b) {
    b.setColour(juce::ToggleButton::textColourId, Ui::text);
    b.setColour(juce::ToggleButton::tickColourId, Ui::accent);
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

void FxPanel::addBlock(Block& b, const char* onParam, const char* typeParam, int types) {
    b.on.setButtonText("On");
    styleToggle(b.on);
    b.typeLabel.setText("Type", juce::dontSendNotification);
    styleLabel(b.typeLabel);
    addAndMakeVisible(b.typeLabel);
    addAndMakeVisible(b.on);
    b.onAtt = std::make_unique<ButtonAtt>(processor_.apvts, onParam, b.on);
    for (int t = 0; t < types; ++t) b.type.addItem("Type " + juce::String(t), t + 1);   // their names once the ROM's here
    addAndMakeVisible(b.type);
    b.typeAtt = std::make_unique<ComboAtt>(processor_.apvts, typeParam, b.type);
}

FxPanel::FxPanel(VSynthEmuProcessor& p) : processor_(p) {
    setOpaque(true);
    styleLabel(status_);
    status_.setFont(juce::Font(juce::FontOptions(11.0f)));
    status_.setColour(juce::Label::textColourId, Ui::text);
    addAndMakeVisible(status_);

    addBlock(mfx_, "fxMfxOn", "fxMfxType", 42);
    addLevel(mfx_.levels, "Master", "fxMfxMaster");
    addLevel(mfx_.levels, "To Chorus", "fxMfxCho");
    addLevel(mfx_.levels, "To Reverb", "fxMfxRev");
    addBlock(chorus_, "fxChoOn", "fxChoType", 9);
    addLevel(chorus_.levels, "Master", "fxChoMaster");
    addLevel(chorus_.levels, "To Reverb", "fxChoRev");
    addBlock(reverb_, "fxRevOn", "fxRevType", 11);
    addLevel(reverb_.levels, "Master", "fxRevMaster");

    addLevel(tone_, "Chorus Send", "tvaChoSend");
    addLevel(tone_, "Reverb Send", "tvaRevSend");
    addLevel(system_, "Master", "fxMaster");
    styleSlider(addLevel(system_, "Output Gain", "fxGain").slider, 52);                  // the one level with a unit
    for (auto* l : { &assignLabel_, &modeLabel_ }) { styleLabel(*l); addAndMakeVisible(*l); }
    assignLabel_.setText("Output Assign", juce::dontSendNotification);
    modeLabel_.setText("Output Mode", juce::dontSendNotification);
    assignBox_.addItemList({ "MULTI", "MAIN", "DIR" }, 1);
    modeBox_.addItemList({ "Mix", "Parallel" }, 1);
    addAndMakeVisible(assignBox_);
    addAndMakeVisible(modeBox_);
    assignAtt_ = std::make_unique<ComboAtt>(processor_.apvts, "outputAssign", assignBox_);
    modeAtt_ = std::make_unique<ComboAtt>(processor_.apvts, "outputMode", modeBox_);

    pageSeg_.onChange = [this](int i) { showPage(i); };
    addAndMakeVisible(pageSeg_);
    view_.setViewedComponent(&grid_, false);
    view_.setScrollBarsShown(true, false);
    addAndMakeVisible(view_);
    buildCards();
    showBlocks();
}

// The page's buttons and the status across the top.  Below, a block's card on the left, or the tone's and the system's on the System page, and on the right the list that page's type or EQ gives.
void FxPanel::buildCards() {
    leftW = kLeftW;
    const auto row = [](juce::Component& l, juce::Component& c) { return Row{ { Item{ &l, Ui::labelW }, Item{ &c, 0 } } }; };
    Card top;
    top.bare = true;
    top.col = -1;
    top.rows = { Row{ { Item{ &pageSeg_, kPageButtonsW }, Item{ &status_, 0 } } } };
    add(top);
    const int firstCard = (int)cards.size();
    const auto blockCard = [&](Block& b, const char* title, int page) {
        Card c { title };
        c.head = { { &b.on, kToggleW } };
        c.rows = { Row{ { Item{ &b.typeLabel, kTypeLabelW }, Item{ &b.type, 0 } } } };
        for (auto& l : b.levels) c.rows.push_back(row(l->label, l->slider));
        c.shown = [this, page] { return page_ == page; };
        if (page > 0) c.sameAs = firstCard;
        add(c);
    };
    blockCard(mfx_, "MFX", 0);
    blockCard(chorus_, "Chorus", 1);
    blockCard(reverb_, "Reverb", 2);
    Card tone { "Tone" };
    tone.sameAs = firstCard;
    tone.rows = { row(tone_[0]->label, tone_[0]->slider), row(tone_[1]->label, tone_[1]->slider), row(assignLabel_, assignBox_), row(modeLabel_, modeBox_) };
    tone.shown = [this] { return page_ == 3; };
    add(tone);
    Card sys { "System" };
    sys.rows = { row(system_[0]->label, system_[0]->slider), row(system_[1]->label, system_[1]->slider) };
    sys.shown = [this] { return page_ == 3; };
    add(sys);
    Card list;
    list.col = 1;
    list.fill = true;
    list.titleFn = [this] { return listTitle_; };
    list.custom = [this](juce::Rectangle<int> a) {
        view_.setBounds(a);
        grid_.setSize(view_.getWidth() - kScrollBarW, grid_.getHeight());
    };
    add(list);
}

FxPanel::~FxPanel() { view_.setViewedComponent(nullptr, false); }

void FxPanel::showPage(int page) {
    page_ = juce::jlimit(0, 3, page);
    pageSeg_.setIndex(page_);
    showBlocks();
    repaint();
    refresh();
}

void FxPanel::showBlocks() {
    const auto show = [](Block& b, bool on) {
        b.on.setVisible(on);
        b.typeLabel.setVisible(on);
        b.type.setVisible(on);
        for (auto& l : b.levels) { l->label.setVisible(on); l->slider.setVisible(on); }
    };
    show(mfx_, page_ == 0);
    show(chorus_, page_ == 1);
    show(reverb_, page_ == 2);
    const bool sys = page_ == 3;
    for (auto* levels : { &tone_, &system_ })
        for (auto& l : *levels) { l->label.setVisible(sys); l->slider.setVisible(sys); }
    assignLabel_.setVisible(sys);
    assignBox_.setVisible(sys);
    modeLabel_.setVisible(sys);
    modeBox_.setVisible(sys);
}

void FxPanel::paint(juce::Graphics& g) {
    g.fillAll(Ui::surfaceApp);
    Ui::Page::paint(g);
}

void FxPanel::resized() {
    Ui::Page::resized();
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

    const int page = page_;
    const int type = page == 0 ? mfx_.type.getSelectedItemIndex() : page == 1 ? chorus_.type.getSelectedItemIndex() : page == 2 ? reverb_.type.getSelectedItemIndex() : 0;
    if (page != shownPage_ || type != shownType_) {
        shownPage_ = page; shownType_ = type;
        static const std::vector<XtFxParam> none;
        const std::vector<std::vector<XtFxParam>>* types = page == 0 ? &cat.mfx : page == 1 ? &cat.chorus : page == 2 ? &cat.reverb : nullptr;
        const std::vector<XtFxParam>& params = !cat.ok ? none : page == 3 ? cat.eq : (size_t)type < types->size() ? (*types)[(size_t)type] : none;
        showParams(params);
        listTitle_ = !cat.ok ? juce::String("The ROM has the names and ranges")
                   : page == 3 ? juce::String("System EQ")
                   : (page == 0 ? mfx_ : page == 1 ? chorus_ : reverb_).type.getText() + (params.empty() ? ": nothing to set" : "");
        repaint();
    }
    for (auto& row : rows_)
        if (row->id >= 0 && !row->value.isMouseButtonDown(true) && !row->value.hasKeyboardFocus(true))
            row->value.setValue(processor_.fxParam(row->id, row->def), juce::dontSendNotification);
}

void FxPanel::showParams(const std::vector<XtFxParam>& params) {
    while (rows_.size() < params.size()) {
        auto row = std::make_unique<ParamRow>();
        styleLabel(row->name);
        styleSlider(row->value, kValueW);
        grid_.addChildComponent(row->name);
        grid_.addChildComponent(row->value);
        rows_.push_back(std::move(row));
    }
    const std::shared_ptr<XtFxValueText> values = processor_.fxCatalog().values;
    for (size_t i = 0; i < rows_.size(); ++i) {
        ParamRow& row = *rows_[i];
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
