#pragma once
// D's WAVE page (v479; research/UI_DESIGN.md, "The pages", WAVE; the user's choice of 2026-11-27, as the XT's SAMPLE
// mode is apart from its patch): the wave's card, OSC 1 and OSC 2 on its title line, a line of what the wave is and
// what the session keeps of it besides its file, and the chosen OSC's wave to edit (WaveEditView); under it the chosen
// OSC's points' card, its Start Offset, Loop, Loop Mode, Loop Start, Loop End, Playback Mode, and the events' Encode
// Type and Depth (v481) from their IDs.  v484 (the user's word, 2026-11-28): -, + and Fit at the right of the wave's
// line with the window it shows, and a key under the wave naming each mark in the colour it's drawn.  Each
// OSC's view and card are made once, by index, and only the chosen one shows.
#include "PluginProcessor.h"
#include "UiCard.h"
#include "UiParts.h"
#include "UiWaveEdit.h"
#include <array>
#include <memory>
#include <vector>

// The marks' key under the wave (v484): each mark as the view draws it, then its name, in WaveEditView::Mark's order
class WaveKey : public juce::Component {
public:
    struct Item {
        WaveEditView::Mark mark;
        juce::Point<float> swatch;     // the mark's centre
        juce::Rectangle<float> name;   // its name's place
    };
    std::vector<Item> items() const;   // where each goes, as paint draws them
    void paint(juce::Graphics& g) override;
};

class WavePage : public juce::Component {
public:
    explicit WavePage(VSynthEmuProcessor& p);
    static constexpr int kOscs = 2;

    void showOsc(int osc);
    int osc() const { return osc_; }
    WaveEditView& view(int osc) { return *views_[(size_t)osc]; }
    // The page's buttons in kButtons' order, for the checks: the title line's (Revert and Truncate, v480; the events',
    // v481) and the wave's line's zoom (v484)
    enum Button : size_t {
        kSave, kRevert, kTruncate, kDeleteEvent, kAddEvent, kNextEvent, kPrevEvent, kZoomOut, kZoomIn, kFit, kButtons
    };
    juce::Button& button(Button b) { return *buttons_[b]; }
    juce::Button& truncateButton() { return button(kTruncate); }
    juce::Button& revertButton() { return button(kRevert); }
    juce::String said() const { return info_.getText(); }    // the wave's line, for the checks
    juce::String shownText() const { return shown_.getText(); }   // the window it shows (v484)
    WaveKey& key() { return key_; }
    void refresh();                                         // the editor's timer, while the page shows
    void resized() override;

private:
    VSynthEmuProcessor& processor_;
    int osc_ = 0;
    Card waveCard_ { "WAVE" };
    Ui::Segmented* oscs_ = nullptr;
    std::array<juce::TextButton*, kButtons> buttons_ {};   // each acting on the shown OSC's view
    std::array<std::unique_ptr<juce::TextButton>, kButtons> onLine_;   // the wave line's, which the page owns
    juce::Label info_, shown_;
    WaveKey key_;
    std::array<std::unique_ptr<WaveEditView>, kOscs> views_;
    std::array<std::unique_ptr<Card>, kOscs> points_;
    juce::String says() const;
};
