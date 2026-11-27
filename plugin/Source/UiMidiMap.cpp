// The map of the controllers learnt (UiMidiMap.h, v463)
#include "UiMidiMap.h"
#include "PluginProcessor.h"
#include "UiLook.h"
#include "UiMidiLearn.h"

namespace {
const char* const kNone = "No controller learnt: right-click a control, then Learn MIDI CC";
const char* const kHow = "Right-click a control to learn or forget its controller";
}

MidiMapView::MidiMapView(VSynthEmuProcessor& p) : processor_(p) {
    Look::setTextLook(title_, Look::sans(12.0f, "Bold", 0.1f), Look::text, juce::Justification::centredLeft);
    title_.setText("MIDI MAP", juce::dontSendNotification);
    Look::setTextLook(waiting_, Look::sans(11.0f), Look::muted, juce::Justification::centredLeft);
    forgetAll_.onClick = [this] { forgetAll(); };
    view_.setViewedComponent(&list_, false);
    view_.setScrollBarsShown(true, false);
    addAndMakeVisible(title_);
    addAndMakeVisible(view_);
    addAndMakeVisible(waiting_);
    addAndMakeVisible(forgetAll_);
    rebuild();
    startTimerHz(10);
}

MidiMapView::~MidiMapView() { stopTimer(); }

void MidiMapView::open(VSynthEmuProcessor& p, juce::Component& at) {
    juce::CallOutBox::launchAsynchronously(std::make_unique<MidiMapView>(p), at.getScreenBounds(), nullptr);
}

void MidiMapView::rebuild() {
    PluginMidiLearn& learnt = processor_.midiIn().learn;
    seen_ = learnt.changes();
    rows_.clear();
    for (const auto& [cc, param] : learnt.entries()) {
        auto row = std::make_unique<MapRow>();
        Look::setTextLook(row->cc, Look::mono(11.0f), Look::accent, juce::Justification::centredLeft);
        row->cc.setText("CC " + juce::String(cc), juce::dontSendNotification);
        Look::setTextLook(row->name, Look::sans(11.0f, "Semibold"), Look::text, juce::Justification::centredLeft);
        row->name.setText(processor_.midiIn().edits.param(param)->getName(64), juce::dontSendNotification);
        row->forget.onClick = [this, param] { processor_.midiIn().learn.forget(param); };
        list_.addAndMakeVisible(row->cc);
        list_.addAndMakeVisible(row->name);
        list_.addAndMakeVisible(row->forget);
        rows_.push_back(std::move(row));
    }
    learning_ = MidiLearn::learning(processor_);
    waiting_.setText(learning_.isNotEmpty() ? learning_ : rows_.empty() ? kNone : kHow, juce::dontSendNotification);
    forgetAll_.setEnabled(!rows_.empty());
    setSize(kWidth, kTitleH + kRowH * juce::jlimit(1, kMostShown, rows()) + kFootH);
    resized();
}

juce::String MidiMapView::rowText(int i) const {
    return rows_[(size_t)i]->cc.getText() + " " + rows_[(size_t)i]->name.getText();
}

juce::String MidiMapView::waitingText() const { return waiting_.getText(); }

void MidiMapView::forgetRow(int i) { rows_[(size_t)i]->forget.onClick(); }   // as a click does, now

void MidiMapView::forgetAll() { processor_.midiIn().learn.forgetAll(); }

void MidiMapView::resized() {
    auto r = getLocalBounds().reduced(8, 4);
    title_.setBounds(r.removeFromTop(kTitleH - 4));
    auto foot = r.removeFromBottom(kFootH - 4);
    forgetAll_.setBounds(foot.removeFromBottom(24).removeFromRight(90));
    waiting_.setBounds(foot);
    view_.setBounds(r);
    list_.setSize(r.getWidth() - (rows() > kMostShown ? view_.getScrollBarThickness() : 0), kRowH * rows());
    for (int i = 0; i < rows(); ++i) {
        auto line = juce::Rectangle<int>(0, i * kRowH, list_.getWidth(), kRowH).reduced(0, 1);
        MapRow& row = *rows_[(size_t)i];
        row.forget.setBounds(line.removeFromRight(64));
        row.cc.setBounds(line.removeFromLeft(52));
        row.name.setBounds(line.withTrimmedRight(6));
    }
}

void MidiMapView::paint(juce::Graphics& g) { g.fillAll(Look::panel); }

void MidiMapView::timerCallback() {
    const bool moved = processor_.midiIn().learn.changes() != seen_;
    if (moved || learning_ != MidiLearn::learning(processor_)) rebuild();
}
