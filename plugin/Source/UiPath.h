#pragma once
// D's signal path (research/UI_DESIGN.md, "The window"; UI step 1): the blocks in their order, each with its setting under
// its name, the selected one filled with the accent; the effects' blocks with ON or OFF; the Structure box at the right.
// A click on a block selects it, one on an effect opens its page.
#include "UiBlocks.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include <memory>
#include <vector>

class PathBar : public juce::Component {
public:
    explicit PathBar(juce::AudioProcessorValueTreeState& state);

    std::function<void(int block)> onSelect;    // an index into Blocks::kPath
    std::function<void(int effect)> onEffect;   // 0 MFX, 1 Chorus, 2 Reverb
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
        std::vector<Chip> blocks, effects;
    };
    Chips laidOut() const;   // where each chip is drawn and clicked

private:
    juce::AudioProcessorValueTreeState& state_;
    juce::ComboBox structure_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> structureAtt_;   // made once the box has its items
    int selected_ = Blocks::kFirstSelected;
    juce::String shown_;     // what the chips said when they were drawn
};
