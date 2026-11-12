#pragma once
#include <juce_core/juce_core.h>

// The Multi Step Modulator's plugin side (MsmPluginTest.cpp, v193), run by VSynthEmuArpPluginTest after the arpeggio's
// checks: 0 if every check holds, its summary the last line
int msmPluginTest(const juce::File& rom, const juce::File& work);
