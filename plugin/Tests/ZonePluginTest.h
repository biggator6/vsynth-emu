#pragma once
#include <juce_core/juce_core.h>

// The zones' plugin side (ZonePluginTest.cpp, v195), run by VSynthEmuArpPluginTest after the arpeggio's and the MSM's
// checks: 0 if every check holds, its summary the last line
int zonePluginTest(const juce::File& rom, const juce::File& work);
