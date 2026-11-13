#pragma once
// A V-Synth wave read from its file, and a project's wave found by its number: what a patch's load and its zones' share
// (PluginProcessor.cpp's loadFile and applyPatch, PluginZones.cpp's loadZones).  Moved from PluginProcessor.cpp as they
// stood (v195), when loadZones went to its own file.
#include <juce_core/juce_core.h>
#include <string>
#include "XtFiles.h"

// The wave's chunks and audio, false (and why) for a file that isn't one (xtWaveRead)
bool readVSynthWave(const juce::File& file, XtWaveFile& wave, std::string* why = nullptr);
// A project keeps its waves in Wave beside Patch, each named by its number; none for 0 or a folder that isn't there
juce::File findWave(const juce::File& folder, int number);
