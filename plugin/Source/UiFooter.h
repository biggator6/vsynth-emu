#pragma once
// D's footer (research/UI_DESIGN.md, "The window"; UI step 1): MONO or POLY and how many zones the patch plays, then a
// keyboard of six octaves with each zone's keys under a band of its colour, from the zones' Range Upper parameters.  The
// keys play (v201): a press sends its note to the synth through the processor's editor queue, its velocity by where on
// the key (soft at the top, full at the bottom), a drag across keys lets each go for the next, a release lets go; the
// held key is lit.  The editor can fold the whole footer away (SynthEditor::showKeyboard).
#include "PluginProcessor.h"
#include <optional>

class Footer : public juce::Component {
public:
    explicit Footer(VSynthEmuProcessor& p) : processor_(p) {}
    ~Footer() override;   // a key held as the editor closes is let go
    void paint(juce::Graphics&) override;
    void refresh();   // repaints if the keys' mode, the zones or their keys have moved since they were drawn
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

    static constexpr int kStatusW = 184;           // as wide as the rack's Time Trip column
    static constexpr int kLowKey = 24, kWhiteKeys = 42;   // C1 and six octaves of white keys up from it
    static juce::Colour zoneColour(int zone);

    // Where a key is drawn, and the key under a point, a black key over the white ones it sits on: the presses' and the
    // checks' (v201)
    juce::Rectangle<float> keyBounds(int key) const;
    std::optional<int> keyAt(juce::Point<float> at) const;
    std::optional<int> held() const { return held_; }

private:
    VSynthEmuProcessor& processor_;
    juce::String shown_;   // what the footer said when it was drawn
    std::optional<int> held_;
    juce::Rectangle<float> keysArea() const;
    float velocityAt(int key, float y) const;
    void press(juce::Point<float> at);   // the held key let go, and the key under `at` pressed
    void letGo();
    void paintZones(juce::Graphics&, juce::Rectangle<float> band, float keyW) const;
    void paintKeys(juce::Graphics&) const;
};
