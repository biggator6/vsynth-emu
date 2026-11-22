#pragma once
// The VC-2 mode's VOCODER page (v453; research/UI_DESIGN_VC2.md, "VOCODER"): the patch's vocoder block and the mic, the
// same for both zones.  MIC IN down the left: External In Type, the mic's state and its checks (UiVc2Mic.h), the first not
// ticked said in full; VOCODER at the top right, its Type and switch on its title, the engine's bands' display (d-14) and
// its levels and times; under it FORMANT (with its warp drawn), CONSONANTS, KEYS & HOLD and AUTO NOTE.  LEGACY and HIRESO
// share it (d-2): what the chosen engine doesn't read is faint, a line saying which engine reads it.  What needs the
// VC-2 synth's view of its state (the input's level, voiced, the tracked pitch, Auto Note's note, the notes sounding,
// the output and the bands) says it waits for it.
#include "PluginProcessor.h"
#include "UiCard.h"
#include "UiControlGrid.h"
#include "UiWatch.h"
#include <memory>
#include <vector>

// A grid of controls and a drawing under it, in one card's body (FORMANT's warp, the PATCH EQ's curve)
class GridAndDrawing : public juce::Component {
public:
    GridAndDrawing(Controls::State& state, const ControlGrid::Rows& rows, std::unique_ptr<juce::Component> drawing);
    ControlGrid& grid() { return grid_; }
    static constexpr int kDrawingH = 100;
    int heightFor(int width) const { return grid_.heightFor(width) + ControlGrid::kGap + kDrawingH; }
    void resized() override;

private:
    ControlGrid grid_;
    std::unique_ptr<juce::Component> drawing_;
};

class Vc2VocoderPage : public juce::Component {
public:
    explicit Vc2VocoderPage(VSynthEmuProcessor& p);
    ~Vc2VocoderPage() override;
    void refresh();   // the editor's timer, while the page shows: the mic's checks, which follow the host's input too
    juce::String status() const;                 // the status line as it says now
    std::vector<juce::String> checkLines() const;   // each check's line as it says now
    void resized() override;

private:
    VSynthEmuProcessor& processor_;
    std::unique_ptr<Card> mic_, vocoder_, formant_, consonants_, keys_, autoNote_;
    std::vector<ControlGrid*> grids_;
    juce::Label engineWord_;
    Watch type_;   // Vocoder Type, which the faint controls follow
    void dimByEngine();
};
