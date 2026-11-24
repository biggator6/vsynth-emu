#pragma once
// A project saved (v603; HARDWARE_FIRMWARE.md, "A project saved"): a new project folder (NAME.prj) holding the browsed
// project's patches, waves (its links among them) and .s00 system files as they are, the current patch saved over its
// own slot (PluginPatchSave.h); and each PCM OSC's wave from outside the project copied in at the next free Wave slot,
// the saved patch's Wave Number on it.  A V-Synth wave (.vpw, an XT .wav, a link) goes as it is, as the XT keeps it; a
// plain WAV or AIFF too, named in the report as one the XT must import and encode itself (the user's choice,
// 2026-11-24).  With no project browsed, the project is the current patch at slot 001 and its waves.  Message thread.
#include <juce_core/juce_core.h>
#include <string>

class VSynthEmuProcessor;

struct ProjectSaved {
    juce::File patch;               // the current patch's file in the new project
    int patches = 0, waves = 0;     // the files the new project holds
    juce::StringArray toImport;     // plain audio the XT must import, by its file's name in the new project
};

// `patch` the browsed patch's file (none for a patch not from one); `dest` the new project's folder, which mustn't be
// there yet.  False (and why) for a folder that's there, a file that can't be copied or written, or a failed save
bool saveProject(const VSynthEmuProcessor& p, const juce::File& patch, const juce::File& dest, ProjectSaved& report,
                 std::string* why = nullptr);
