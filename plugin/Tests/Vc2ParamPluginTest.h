#pragma once
// The VC-2's host parameters (v451): VSynthEmuArpPluginTest's part for them, Vc2ParamPluginTest.cpp.
#include <juce_core/juce_core.h>

// On rom.2's image and rom.4's beside it; 0 when every check holds
int vc2ParamPluginTest(const juce::File& rom, const juce::File& work);
// The child's run without rom.4 (`<rom> vc2ids`): each VC-2 parameter's ID and range, one a line
int vc2ParamStandIns(const juce::File& rom, const juce::File& work);
