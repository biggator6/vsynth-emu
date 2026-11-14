// The plugin's own folder and its settings (PluginAppFolder.h), moved from PluginProcessor.cpp as they stood (v198).
#include "PluginAppFolder.h"

#include <cstdlib>

// What belongs to the plugin and not to a project, whichever format is running: ~/Library/Application Support/V-Synth Emu,
// or VSE_APP_DIR, so that PluginTest leaves the user's alone.
juce::File appDir() {
    if (const char* d = std::getenv("VSE_APP_DIR")) return juce::File(juce::String::fromUTF8(d));
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Application Support/V-Synth Emu");
}

std::unique_ptr<juce::PropertiesFile> appSettings() {
    juce::PropertiesFile::Options o;
    o.millisecondsBeforeSaving = -1;   // only when told
    return std::make_unique<juce::PropertiesFile>(appDir().getChildFile("settings.xml"), o);
}

void keepSetting(const char* key, const juce::String& value) {
    appDir().createDirectory();
    auto settings = appSettings();
    settings->setValue(key, value);
    settings->save();
}
