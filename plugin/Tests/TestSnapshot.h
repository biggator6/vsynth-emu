#pragma once
// A component drawn offscreen at twice its size into a PNG: PluginTest's TEST_SNAPSHOT, and VSynthEmuEditorTest's pages
// (moved from PluginTest.cpp's main when D's test came to need it, UI step 1).  False if the file can't be written.
#include <juce_gui_basics/juce_gui_basics.h>
#include <cstdio>

inline bool writeSnapshot(juce::Component& c, const juce::File& f) {
    const juce::Image img = c.createComponentSnapshot(c.getLocalBounds(), true, 2.0f);
    f.deleteFile();
    juce::FileOutputStream os(f);
    juce::PNGImageFormat png;
    if (!os.openedOk() || !png.writeImageToStream(img, os)) return false;
    std::printf("snapshot: %s (%dx%d)\n", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    return true;
}
