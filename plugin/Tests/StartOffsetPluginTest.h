#pragma once
#include <juce_core/juce_core.h>

// Start Offset's text in time (StartOffsetPluginTest.cpp, v478; the user's d-7), run by VSynthEmuArpPluginTest after
// the VC-2's parameters: 0 if every check holds, its summary the last line
int startOffsetPluginTest(const juce::File& rom, const juce::File& work);
