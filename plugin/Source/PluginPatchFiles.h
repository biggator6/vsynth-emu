#pragma once
// A V-Synth wave read from its file, and a project's wave found by its number: what a patch's load and its zones' share
// (PluginProcessor.cpp's loadFile and applyPatch, PluginZones.cpp's loadZones).  Moved from PluginProcessor.cpp as they
// stood (v195), when loadZones went to its own file.  A project's link to a preset wave (.w00) is followed to it (v600).
#include <juce_core/juce_core.h>
#include <string>
#include "XtFiles.h"

// A V-Synth wave by its first chunks: a .vpw, a project's .wav (the same chunks in a RIFF; a LITE wave's has no vpfl, its
// vpsc after the fmt chunk), or a project's link to a preset wave (v600: moved here from PluginProcessor.cpp)
bool isVSynthWave(const juce::File& file);
// The wave's chunks and audio, false (and why) for a file that isn't one (xtWaveRead); a link's preset wave from the disk
// image beside the firmware (PluginPresetWaves.h)
bool readVSynthWave(const juce::File& file, XtWaveFile& wave, std::string* why = nullptr);
// A project keeps its waves in Wave beside Patch, each named by its number, as a wave or a link to one; none for 0 or a
// folder that isn't there
juce::File findWave(const juce::File& folder, int number);
