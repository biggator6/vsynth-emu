#pragma once
// The MSM tab's controls (v193): the Multi Step Modulator's Master Switch and Key Sync, and one track at a time, picked
// by the Track box as the Matrix's Controls are: the track's eight settings and its 16 steps.  VSynthEmuEditor::
// buildMsmPage places them (PluginEditorMsm.cpp); attachTrack puts a track's parameters on them.
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <memory>
#include <utility>
#include <vector>

struct MsmControls {
    static constexpr int kTracks = 4, kSteps = 16;
    using Apvts = juce::AudioProcessorValueTreeState;

    juce::ToggleButton on { "On" }, trackOn { "On" }, loop { "Loop" };
    juce::ComboBox keySync, track, type, grid, direction, dest;
    juce::Slider endPoint, zone;
    std::array<juce::Slider, kSteps> steps;
    std::array<juce::Label, kSteps> stepLabels;      // the step's number
    juce::Label keySyncLabel { {}, "Key sync" }, trackLabel { {}, "Track" }, typeLabel { {}, "Type" },
        gridLabel { {}, "Grid" }, endPointLabel { {}, "End point" }, directionLabel { {}, "Direction" },
        zoneLabel { {}, "Zone" }, destLabel { {}, "Destination" };
    int shown = 0;                                   // the track on the controls, 0…3
    // The Master Switch's and Key Sync's, made once; the track's, made again for each
    std::unique_ptr<Apvts::ButtonAttachment> onAtt;
    std::unique_ptr<Apvts::ComboBoxAttachment> keySyncAtt;
    std::vector<std::unique_ptr<Apvts::ButtonAttachment>> trackButtons;
    std::vector<std::unique_ptr<Apvts::ComboBoxAttachment>> trackBoxes;
    std::vector<std::unique_ptr<Apvts::SliderAttachment>> trackSliders;

    void make(Apvts& apvts);                          // the boxes' items, the styles, the step numbers
    void attachWhole(Apvts& apvts);                   // the Master Switch and Key Sync
    void attachTrack(Apvts& apvts, int t);            // track t's 24
    std::vector<juce::Component*> controls();
    std::vector<juce::Label*> labels();
    juce::String title(const char* what) const;       // "Track n" and what follows, for a card that follows the track

private:
    // A track's boxes, each with its setting's place after the track's Switch: Type 2, Grid 3, Direction 5, Destination 7
    std::array<std::pair<juce::ComboBox*, int>, 4> trackBoxList();
    std::vector<juce::Slider*> sliders();
};
