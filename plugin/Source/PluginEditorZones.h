#pragma once
// The Zones tab (v195): the sixteen zones' Range Upper keys that split the keyboard, each a slider on its parameter, and a map
// of the keys each zone plays and what it plays, or why it is silent.  VSynthEmuEditor::buildZonesPage places them
// (PluginEditorZones.cpp).
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <memory>
#include <vector>
#include "XtSynthZones.h"

// The keyboard's 128 keys under the zones that play them, each zone's keys marked and numbered, and a line a zone of what it
// plays.  Drawn from the sliders' values and the processor's zonePlays when either changes; nothing runs while it is hidden.
class ZoneMap final : public juce::Component {
public:
    int upper[kXtZones] = {};           // the splits as the sliders have them
    juce::StringArray plays;            // the processor's zonePlays: a line a zone, zone 1's empty; none without a patch
    void paint(juce::Graphics& g) override;

private:
    void paintKeys(juce::Graphics& g, juce::Rectangle<int> strip) const;
    void paintLines(juce::Graphics& g, juce::Rectangle<int> area) const;
    juce::String linesOf(int zone) const;
};

struct ZonesControls : private juce::Value::Listener {
    using Apvts = juce::AudioProcessorValueTreeState;
    static constexpr int kMapH = 110;   // the map's keys, their octaves, and four rows of four zones' lines

    ZonesControls() { plays.addListener(this); }
    ~ZonesControls() override { plays.removeListener(this); }
    ZonesControls(const ZonesControls&) = delete;
    ZonesControls& operator=(const ZonesControls&) = delete;

    std::array<juce::Slider, kXtZones> upper;
    std::array<juce::Label, kXtZones> names;   // "Zone n"
    ZoneMap map;
    std::vector<std::unique_ptr<Apvts::SliderAttachment>> atts;
    juce::Value plays;                          // the processor's "zonePlays", to hear a patch's zones come in

    void make(Apvts& apvts);                    // styled, named, on their parameters, and heard
    void refresh();                             // the map from the sliders and what the zones play

private:
    void valueChanged(juce::Value&) override { refresh(); }
};
