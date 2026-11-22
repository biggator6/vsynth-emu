#pragma once
// A VC-2 patch's load and its session (v452): VSynthEmuArpPluginTest's part for them, Vc2PatchPluginTest.cpp.
#include <juce_core/juce_core.h>

// On rom.2's image, with rom.4 and the disk image beside it; 0 when every check holds
int vc2PatchPluginTest(const juce::File& rom, const juce::File& work);
