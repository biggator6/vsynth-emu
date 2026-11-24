#pragma once
// The patch box's Save… (v602; research/UI_DESIGN.md, "The patch box", the user's choice of 2026-11-24): a menu of Save,
// over the patch's own file once asked, and Save As…, a file to choose with the project's next free slot offered and
// the patch named after it.  Each writes by the mode's save; what was written is told back, and a failure said.
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
};

// The menu, under `target`; `chooser` keeps Save As…'s file chooser while it's open
void showPatchSaveMenu(juce::Component& target, const PatchSaving& s, std::unique_ptr<juce::FileChooser>& chooser);
// A project's next free slot in `folder` ("NNN name.ext", one past its highest), the file Save As… offers
juce::File nextSlotFile(const juce::File& folder, const juce::String& name, const juce::String& extension);
// A patch's name from its file's ("002 HouseOfWax" gives "HouseOfWax"; a name with no slot is its own)
juce::String patchNameOfFile(const juce::File& file);
