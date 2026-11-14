#pragma once
// What belongs to the plugin and not to a project, whichever format is running (moved from PluginProcessor.cpp as it
// stood, v198): its folder, and the settings kept there: the image last chosen (PluginRom.cpp) and the last patch
// folder (PluginPatchFiles.cpp).  PluginAppFolder.cpp.
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <memory>

// ~/Library/Application Support/V-Synth Emu, or VSE_APP_DIR, so that PluginTest leaves the user's alone
juce::File appDir();
// settings.xml there, saved only when told
std::unique_ptr<juce::PropertiesFile> appSettings();
// `value` kept under `key` there for every project and plugin format, the folder made if it isn't yet (v198: the two
// images chosen and the last patch folder)
void keepSetting(const char* key, const juce::String& value);
