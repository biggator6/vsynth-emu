#pragma once
// The VC-2 mode's FX page (v453; research/UI_DESIGN_VC2.md, "FX"): the patch's MFX, Chorus and Reverb, each its switch on
// its title, its Type and levels; the System card (Master Level, Output Gain); and the PATCH EQ, its two bands' curve
// drawn over their controls (d-5).  It is D's cards, not FxPanel: the types' own parameters, edits by ID as the
// V-Synth's (the user's scope, 2026-11-21), need rom.4's effect driver in the plugin, which comes with the VC-2 synth's
// effects (step 4.3's wiring), and a card says so; until then a Type shows its number.
#include "PluginProcessor.h"
#include "UiCard.h"
#include <memory>
#include <vector>

class Vc2FxPage : public juce::Component {
public:
    explicit Vc2FxPage(VSynthEmuProcessor& p);
    void resized() override;

private:
    std::vector<std::unique_ptr<Card>> left_, right_;
};
