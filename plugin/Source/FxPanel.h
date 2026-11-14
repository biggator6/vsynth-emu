#pragma once
// The editor's FX page: the chip's blocks, and each type's parameters as the firmware's descriptors give them, on the design's cards.  The names, ranges and values' text come from rom.2 at run time, so the page is
// empty of them until the ROM's loaded.  A slider still holds the firmware's raw number and sends that: its text is all that's the XT's own (XtFxValueText).
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <vector>
#include "UiParts.h"

class VSynthEmuProcessor;
struct XtFxParam;

class FxPanel : public Ui::Page, private juce::Timer {
public:
    // Drawn in the palette it is given: D's FX page gives Look::cards (UI step 4)
    FxPanel(VSynthEmuProcessor& p, const Ui::Palette& palette);
    ~FxPanel() override;

    void showPage(int page);                        // 0 MFX, 1 Chorus, 2 Reverb, 3 System: the tone's sends, the outputs and the system's EQ
    int page() const { return page_; }

    void paint(juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct Level { juce::Label label; juce::Slider slider; std::unique_ptr<SliderAtt> att; };
    struct Block { juce::ToggleButton on; juce::Label typeLabel; juce::ComboBox type; std::unique_ptr<ButtonAtt> onAtt; std::unique_ptr<ComboAtt> typeAtt; std::vector<std::unique_ptr<Level>> levels; };
    struct ParamRow { juce::Label name; juce::Slider value; int id = -1, def = 0; };

    void timerCallback() override;
    void refresh();
    void showParams(const std::vector<XtFxParam>& params);
    void showBlocks();                              // the cards of the page that's open
    void buildCards();
    Level& addLevel(std::vector<std::unique_ptr<Level>>& to, const char* text, const char* parameter);
    void addBlock(Block& b, const char* onParam, const char* typeParam, int types);

    VSynthEmuProcessor& processor_;
    juce::Label status_;
    Block mfx_, chorus_, reverb_;
    std::vector<std::unique_ptr<Level>> tone_, system_;
    juce::Label assignLabel_, modeLabel_;
    juce::ComboBox assignBox_, modeBox_;
    std::unique_ptr<ComboAtt> assignAtt_, modeAtt_;
    Ui::Segmented pageSeg_;   // MFX, Chorus, Reverb, System, in the panel's palette
    juce::String listTitle_;
    juce::Viewport view_;
    juce::Component grid_;
    std::vector<std::unique_ptr<ParamRow>> rows_;
    int page_ = 0, shownPage_ = -1, shownType_ = -1;
    bool named_ = false;
};
