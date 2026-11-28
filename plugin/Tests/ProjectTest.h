#pragma once
// VSynthEmuProjectTest's parts (ProjectTest.cpp, v600 on): a project's own files through the processor.
#include "TestChecks.h"
#include "../Source/PluginProcessor.h"
#include "../Source/XtFiles.h"
#include <juce_core/juce_core.h>
#include <optional>

// A patch saved (ProjectSaveTest.cpp, v602): every patch of the image's, applied and saved unchanged, is its file byte
// for byte; one value of each kind moved (a tone's knob, COSM 2's typed cutoff, an effect's knob, the MFX type's own
// parameter, the name) lands in the saved file and moves no other part of it; Save As's next slot and the name it takes
void projectSaveChecks(VSynthEmuProcessor& p, const juce::File& rom, const juce::File& work, TestChecks& check);
// A project saved (ProjectSaveAsTest.cpp, v603), `p` holding `patch`, a patch of a project of one link: saved
// unchanged, its files byte for byte; a folder there already refused; an outside plain WAV in at the next slot, named
// for the XT to import, the saved patch on it; a patch from no project alone at slot 001
void projectSaveAsChecks(VSynthEmuProcessor& p, const juce::File& patch, const juce::File& rom,
                         const juce::File& work, TestChecks& check);
// The patch as the processor would save it now, read back by the firmware's reader (ProjectSaveTest.cpp)
std::optional<XtPatchFile> savedNow(const VSynthEmuProcessor& p, const juce::String& name);
// The PATCH Init (ProjectInitTest.cpp, v473): on a processor holding an image patch of several zones and a project's
// patch browsed, and on a fresh one: the firmware's initialized patch (xtFwPatchInit, which xt_files holds to the
// window's own EXECUTE): its name, its zones, zone 1's controls and its arpeggio as its store gives them, the store kept,
// and saved unchanged the init's store byte for byte, so every parameter is the firmware's; no wave, the patch browsed
// as it was
void projectInitChecks(const juce::File& rom, const juce::File& work, TestChecks& check);
