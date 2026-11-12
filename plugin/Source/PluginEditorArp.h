#pragma once
// The Play tab's Arpeggio cards (v190): the arpeggio's ten settings, each on its parameter, and the pattern card's title
// saying so when the pattern has no step to play.  VSynthEmuEditor::addArpCards builds and places them
// (PluginEditorPlay.cpp).
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include <memory>
#include <vector>

struct ArpControls : private juce::Value::Listener {
    ArpControls() { pattern.addListener(this); }
    ~ArpControls() override { pattern.removeListener(this); }
    ArpControls(const ArpControls&) = delete;
    ArpControls& operator=(const ArpControls&) = delete;

    juce::ToggleButton on { "On" }, hold { "Hold" };
    juce::ComboBox motif, grid, resolution;
    juce::Slider duration { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider endPoint { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider shuffle { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider velocity { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider octave { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Label motifLabel { {}, "Motif" }, octaveLabel { {}, "Octaves" }, velocityLabel { {}, "Velocity" },
        durationLabel { {}, "Duration" }, gridLabel { {}, "Grid" }, endPointLabel { {}, "End point" },
        shuffleLabel { {}, "Shuffle" };   // its Resolution beside it
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAtts;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAtts;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAtts;
    juce::Value pattern;                 // the processor's "arpPattern", to hear a patch's or a session's come in
    std::function<void()> onPattern;

private:
    void valueChanged(juce::Value&) override {
        if (onPattern) onPattern();
    }
};
