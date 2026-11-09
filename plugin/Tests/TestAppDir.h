#pragma once
#include <juce_core/juce_core.h>
#include <cstdlib>

// VSE_APP_DIR for the whole run: no test reads or writes the user's settings, or finds their image.
struct TestAppDir {
    juce::File dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                         .getNonexistentChildFile("vse-plugintest", "");
    TestAppDir() { setenv("VSE_APP_DIR", dir.getFullPathName().toRawUTF8(), 1); }
    ~TestAppDir() { dir.deleteRecursively(); }
};
