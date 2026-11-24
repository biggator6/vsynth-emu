#pragma once
// The patch box's Save… (v602; research/UI_DESIGN.md, "The patch box", the user's choice of 2026-11-24): a menu of Save,
// over the patch's own file once asked, and Save As…, a file to choose with the project's next free slot offered and
// the patch named after it; and (v603) Save Project As…, a new project's folder to name, what it holds said after.  Each
// writes by the mode's save; the patch's file written is told back, and a failure said.
#include "PluginProjectSave.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <memory>
#include <string>

struct PatchSaving {
    juce::File own;                // the patch's own file, none for a patch not from one
    juce::File folder;             // where Save As… opens: the project's Patch folder, or the last patch's
    juce::String name;             // the patch's name now
    juce::String extension;        // its kind's files'
    std::function<bool(const juce::String& name, const juce::File& file, std::string* why)> save;
    std::function<void(const juce::File& file)> saved;
    // The project's save (v603), none for a mode without one: into a new project's folder
    std::function<bool(const juce::File& dest, ProjectSaved& report, std::string* why)> saveProject;
};

// The menu, under `target`; `chooser` keeps Save As…'s file chooser while it's open
void showPatchSaveMenu(juce::Component& target, const PatchSaving& s, std::unique_ptr<juce::FileChooser>& chooser);
