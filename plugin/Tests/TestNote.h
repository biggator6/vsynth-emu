#pragma once
#include "../Source/PluginProcessor.h"
#include <vector>

// Middle C at velocity 100 from the first block, on a processor prepared at 48 kHz: `blocks` blocks of `block` frames,
// the first `channels` channels of each block in turn.  The tests' one way to hear a processor.
std::vector<float> playNote(VSynthEmuProcessor& p, int block, int blocks, int channels);

// `wave` loaded into OSC 1 and encoded as Encode Type `type` (its choice index: 0 Auto, 1 SOLO, 2 BACKING): the status
// line once it says "Encoded" or "Encode failed", or as it stands after `seconds`.
juce::String encodeWave(VSynthEmuProcessor& p, const juce::File& wave, float type, int seconds = 30);
