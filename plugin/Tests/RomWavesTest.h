#pragma once
// VSynthEmuRomTest's checks of the VC-2's waves at the plugin's start (v504, RomWavesTest.cpp)
#include "TestChecks.h"
#include <juce_core/juce_core.h>
#include <string>

// The waves this process's start loaded from the disk image beside rom.4 (rom.2 at `rom`), against rom.4's own load of
// the same files, and two fresh runs: one ending as the load runs, which must end at once
void wavesAtStart(TestChecks& check, const juce::File& rom);
// A fresh run's part by its argument, what it prints, and what it gives back: -1 when `mode` isn't one of these
int wavesChild(const juce::String& mode);
