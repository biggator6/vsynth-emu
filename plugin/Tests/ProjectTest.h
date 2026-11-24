#pragma once
// VSynthEmuProjectTest's parts (ProjectTest.cpp, v600 on): a project's own files through the processor.
#include "TestChecks.h"
#include "../Source/PluginProcessor.h"
#include <juce_core/juce_core.h>

// A patch saved (ProjectSaveTest.cpp, v602): every patch of the image's, applied and saved unchanged, is its file byte
// for byte; one value of each kind moved (a tone's knob, COSM 2's typed cutoff, an effect's knob, the MFX type's own
// parameter, the name) lands in the saved file and moves no other part of it; Save As's next slot and the name it takes
void projectSaveChecks(VSynthEmuProcessor& p, const juce::File& rom, const juce::File& work, TestChecks& check);
