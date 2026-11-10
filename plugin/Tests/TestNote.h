#pragma once
#include "../Source/PluginProcessor.h"
#include <vector>

// Middle C at velocity 100 from the first block, on a processor prepared at 48 kHz: `blocks` blocks of `block` frames,
// the first `channels` channels of each block in turn.  The tests' one way to hear a processor.
std::vector<float> playNote(VSynthEmuProcessor& p, int block, int blocks, int channels);
