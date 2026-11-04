#pragma once
// The effects' page: the chip's blocks, and each type's parameters as the firmware's descriptors give them.  The names and ranges come from rom.2 at run time, so the page is empty of them until the
// ROM's loaded.  Values are the firmware's raw numbers: its Delay Time of 112 is a note value here too, shown as 112.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <vector>

class VSynthEmuProcessor;
struct XtFxParam;

class FxPanel : public juce::Component, private juce::Timer {
public:
    explicit FxPanel(VSynthEmuProcessor& p);
    ~FxPanel() override;

    void showPage(int page);                        // 0 MFX, 1 Chorus, 2 Reverb, 3 the system's EQ

    void paint(juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct Level { juce::Label label; juce::Slider slider; std::unique_ptr<SliderAtt> att; };
    struct Block { juce::ToggleButton on; juce::ComboBox type; std::unique_ptr<ButtonAtt> onAtt; std::unique_ptr<ComboAtt> typeAtt; std::vector<std::unique_ptr<Level>> levels; };
    struct Row { juce::Label name; juce::Slider value; int id = -1, def = 0; };

    void timerCallback() override;
    void refresh();
    void showParams(const std::vector<XtFxParam>& params);
    Level& addLevel(std::vector<std::unique_ptr<Level>>& to, const char* text, const char* parameter);
    void addBlock(Block& b, const char* name, const char* onParam, const char* typeParam, int types);
    void layoutLevels(juce::Rectangle<int> row, std::vector<std::unique_ptr<Level>>& levels, int across);

    VSynthEmuProcessor& processor_;
    juce::Label status_;
    Block mfx_, chorus_, reverb_;
    std::vector<std::unique_ptr<Level>> tone_, system_;
    juce::Label toneLabel_, systemLabel_, assignLabel_, modeLabel_;
    juce::ComboBox assignBox_, modeBox_;
    std::unique_ptr<ComboAtt> assignAtt_, modeAtt_;
    juce::Label pageLabel_;
    juce::ComboBox pageBox_;
    juce::Viewport view_;
    juce::Component grid_;
    std::vector<std::unique_ptr<Row>> rows_;
    int shownPage_ = -1, shownType_ = -1;
    bool named_ = false;
    int ruleY_ = 0;
};
