#pragma once
// Parameters a drawing follows (UI step 3): an envelope's curve, an LFO's shape.  Each one's value as last said, on the
// message thread, and a call when one moves, so a drawing repaints when what it shows has changed and only then; and a
// drag's gesture, so a point dragged on a drawing sets its parameter as a control does.
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include <memory>
#include <vector>

class Watch {
public:
    Watch(juce::AudioProcessorValueTreeState& state, const juce::StringArray& ids, std::function<void()> moved);

    bool complete() const;                 // every ID named a parameter (the editor's test reports a table's mistake)
    float value(size_t i) const;           // as the parameter counts it
    float proportion(size_t i) const;      // of its range, 0 to 1
    void begin(size_t i);                  // a drag's start, its moves and its end
    void drag(size_t i, float proportion);
    void end(size_t i);

private:
    std::vector<juce::RangedAudioParameter*> params_;
    std::vector<std::unique_ptr<juce::ParameterAttachment>> atts_;
    std::vector<float> values_;
    std::function<void()> moved_;
};
