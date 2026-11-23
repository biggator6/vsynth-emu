#pragma once
// The Multi Step Modulator on the MODULATION page (research/UI_DESIGN.md, "The pages"; UI step 4): its switch and Key
// Sync, a tab for each track, and the chosen track.  A track is made for each by index (msmTrackFwId): its eight
// settings as a ControlGrid, and its 16 steps as upright bars to drag, those past its End Point dimmed.
#include "UiBars.h"
#include "UiControlGrid.h"
#include "UiTabs.h"
#include "UiWatch.h"
#include <array>
#include <memory>
#include <vector>

class MsmTrack : public juce::Component {
public:
    MsmTrack(Controls::State& state, int track);
    static constexpr int kSettings = 8, kSteps = 16;
    int made() const { return settings_.made() + steps_.made(); }   // 24 with every ID found
    void resized() override;

private:
    ControlGrid settings_;
    UprightBars steps_;   // each its number under it (UiBars.h, v453)
    Watch end_;           // the End Point, past which the steps are dimmed
    void dimPast();
};

class MsmView : public juce::Component {
public:
    explicit MsmView(Controls::State& state);
    static constexpr int kTracks = 4;
    void showTrack(int t);
    int shown() const { return tabs_.chosen(); }
    MsmTrack& track(int t) { return *tracks_[(size_t)t]; }
    Tabs& tabs() { return tabs_; }
    void resized() override;

private:
    Controls::Attached attached_;
    Tabs tabs_;
    juce::ToggleButton on_ { "MSM" };
    Field keySync_;
    std::array<std::unique_ptr<MsmTrack>, kTracks> tracks_;
};
