#pragma once
// D's signal path (research/UI_DESIGN.md, "The window"; UI step 1): the blocks in their order, each with its setting under
// its name, the selected one filled with the accent; the effects' blocks with ON or OFF; the Structure box at the right.
// A click on a block selects it, one on an effect opens its page.  What it shows is a mode's (v453): the V-Synth's, or
// the VC-2's with its zones' chips first, the selected zone's filled in its colour, and a VOCODER chip after AMP
// (research/UI_DESIGN_VC2.md, "SYNTH").
#include "UiBlocks.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <functional>
#include <memory>
#include <vector>

class PathBar : public juce::Component {
public:
    using State = juce::AudioProcessorValueTreeState;
    // An effect's chip: its name and the switch it says ON or OFF of
    struct Effect {
        const char* name;
        juce::String switchId;
    };
    // A mode's path: its blocks and each one's line under its name, its effects, the Structure parameter, and the VC-2's
    // Current Zone (its chips) and VOCODER chip's line (none for the V-Synth)
    struct Mode {
        Blocks::Path path;
        juce::String (*setting)(State&, const Blocks::Block&);
        std::array<Effect, 3> effects;
        juce::String structureId, zoneId;
        juce::String (*vocoder)(State&) = nullptr;
    };
    static Mode vsynth();

    explicit PathBar(State& state, const Mode& mode = vsynth());

    std::function<void(int block)> onSelect;    // an index into Blocks::kPath
    std::function<void(int effect)> onEffect;   // 0 MFX, 1 Chorus, 2 Reverb
    std::function<void()> onVocoder;            // the VOCODER chip
    void setSelected(int block);
    int selected() const { return selected_; }
    void refresh();   // repaints if a block's setting or an effect's switch has moved since it was drawn

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;

    // A block or an effect as drawn: its name, the setting under it, where it is, whether it is on
    struct Chip {
        juce::String name, setting;
        juce::Rectangle<int> r;
        bool on = true;
    };
    struct Chips {
        std::vector<Chip> zones, blocks, vocoder, effects;
    };
    Chips laidOut() const;   // where each chip is drawn and clicked

private:
    State& state_;
    Mode mode_;
    juce::ComboBox structure_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> structureAtt_;   // made once the box has its items
    int selected_ = Blocks::kFirstSelected;
    juce::String shown_;     // what the chips said when they were drawn
    int zone() const;        // the VC-2's zone now, 0 with none
};
