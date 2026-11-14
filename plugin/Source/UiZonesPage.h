#pragma once
// D's ZONES page (research/UI_DESIGN.md, "The pages"; UI step 4): the 128 keys with each zone's range in its colour (the
// footer's), as a note picks its zone (xtZoneKeys); and a cell for each of the sixteen zones, made by index: its colour
// and name, its Range Upper as a bar to drag, the keys it plays, and what it plays (the processor's zonePlays) or why
// it is silent.  Drawn again when a split moves or a patch brings its zones.
#include "PluginProcessor.h"
#include "UiCard.h"
#include "UiFields.h"
#include "UiWatch.h"
#include "XtSynthZones.h"
#include <array>
#include <memory>

// The keys and the zones' ranges on them
class ZoneKeys : public juce::Component {
public:
    int upper[kXtZones] = {};   // the splits as their parameters have them
    void paint(juce::Graphics&) override;
};

class ZoneCell : public juce::Component {
public:
    ZoneCell(Controls::State& state, int zone);
    Bar& upper() { return upper_; }
    void show(const XtZoneKeys& keys, const juce::String& plays);   // its keys and what it plays, drawn under the bar
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    int zone_;
    Bar upper_;
    juce::String keys_, plays_;
};

class ZonesPage : public juce::Component, private juce::Value::Listener {
public:
    explicit ZonesPage(VSynthEmuProcessor& p);
    ~ZonesPage() override;
    ZonesPage(const ZonesPage&) = delete;
    ZonesPage& operator=(const ZonesPage&) = delete;
    ZoneCell& cell(int zone) { return *cells_[(size_t)zone]; }
    void resized() override;

private:
    VSynthEmuProcessor& processor_;
    std::unique_ptr<Card> keysCard_, zonesCard_;
    ZoneKeys* keys_ = nullptr;
    std::array<std::unique_ptr<ZoneCell>, kXtZones> cells_;
    juce::Value plays_;   // the processor's "zonePlays", to hear a patch's zones come in
    Watch splits_;        // the sixteen Range Uppers
    void refresh();
    static juce::String playsOf(const juce::StringArray& plays, int zone);
    void valueChanged(juce::Value&) override { refresh(); }
};
