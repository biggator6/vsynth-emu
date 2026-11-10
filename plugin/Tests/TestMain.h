#pragma once
#include <juce_core/juce_core.h>

// A test's run: the image its first argument names, and a folder of its own to work in
using TestRun = int (*)(const juce::File& rom, const juce::File& work);

// A test program's main: the usage if no image is named, else `run` under JUCE, in an app folder this run has to itself
// (TestAppDir.h), its work folder `name` in it.  Exits as the run does.
int testMain(int argc, char** argv, const char* name, TestRun run);
