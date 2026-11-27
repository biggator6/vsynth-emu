#pragma once
// D's WAVE page (v479; research/UI_DESIGN.md, "The pages", WAVE; the user's choice of 2026-11-27, as the XT's SAMPLE
// mode is apart from its patch): the wave's card, OSC 1 and OSC 2 on its title line, a line of what the wave is and
// what the session keeps of it besides its file, and the chosen OSC's wave to edit (WaveEditView); under it the chosen
// OSC's points' card, its Start Offset, Loop, Loop Mode, Loop Start, Loop End, Playback Mode, and the events' Encode
// Type and Depth (v481) from their IDs.  Each
// OSC's view and card are made once, by index, and only the chosen one shows.
#include "PluginProcessor.h"
#include "UiCard.h"
#include "UiParts.h"
#include "UiWaveEdit.h"
#include <array>
#include <memory>

class WavePage : public juce::Component {
public:
    explicit WavePage(VSynthEmuProcessor& p);
    static constexpr int kOscs = 2;

    void showOsc(int osc);
    int osc() const { return osc_; }
    WaveEditView& view(int osc) { return *views_[(size_t)osc]; }
    // The title line's buttons (Revert and Truncate, v480; the events', v481), in kButtons' order, for the checks
    enum Button : size_t { kRevert, kTruncate, kDeleteEvent, kAddEvent, kNextEvent, kPrevEvent, kButtons };
    juce::Button& button(Button b) { return *buttons_[b]; }
    juce::Button& truncateButton() { return button(kTruncate); }
    juce::Button& revertButton() { return button(kRevert); }
    juce::String said() const { return info_.getText(); }   // the wave's line, for the checks
    void refresh();                                         // the editor's timer, while the page shows
    void resized() override;

private:
    VSynthEmuProcessor& processor_;
    int osc_ = 0;
    Card waveCard_ { "WAVE" };
    Ui::Segmented* oscs_ = nullptr;
    std::array<juce::TextButton*, kButtons> buttons_ {};   // each acting on the shown OSC's view
    juce::Label info_;
    std::array<std::unique_ptr<WaveEditView>, kOscs> views_;
    std::array<std::unique_ptr<Card>, kOscs> points_;
    juce::String says() const;
};
