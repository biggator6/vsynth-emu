#pragma once
// The plugin's output always numbers (v454): what processBlock hands the host holds no NaN and no infinity.  A sample
// that isn't a finite number becomes 0.  A NaN handed to a host spreads through its mix and stays in its effects'
// feedback until they're reset.  On an XT the voices' floats become integers before they're heard, so a NaN there is
// some fixed value; 0 stands in for it until the user's XT says which.  Step 4.3's v349 found the first way to one: a
// sounding VOCODER OSC whose Type changes to PCM (VC2_MODE_PLAN.md, step 4.5's "A NaN sample to the host").
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

// Made at the top of processBlock, so every way out of it goes through: the buffer set right as the block ends.
// Real-time: no allocation, one pass over the block.
class OutputFinite final {
public:
    explicit OutputFinite(juce::AudioBuffer<float>& buffer) : buffer_(buffer) {}
    ~OutputFinite() {
        if (buffer_.hasBeenCleared()) return;
        for (int ch = 0; ch < buffer_.getNumChannels(); ++ch) {
            float* x = buffer_.getWritePointer(ch);
            for (int i = 0; i < buffer_.getNumSamples(); ++i)
                if (!std::isfinite(x[i])) x[i] = 0.0f;
        }
    }
    OutputFinite(const OutputFinite&) = delete;
    OutputFinite& operator=(const OutputFinite&) = delete;

private:
    juce::AudioBuffer<float>& buffer_;
};
