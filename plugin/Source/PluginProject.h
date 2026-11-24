#pragma once
// A project's patches, to browse (v601; research/UI_DESIGN.md, "The patch box"): the XT keeps a project as a folder
// (NAME.prj, a VC-2's NAME.vc2) holding Patch, its patches each named by its slot ("002 HouseOfWax.p00"), and Wave
// beside it.  A patch's project is its Patch folder's parent; a patch in a folder of any other name is browsed with
// that folder's.  By file name, so by slot; a patch kind by its extension, a V-Synth's p00 or a VC-2's p02.  No firmware.
#include <juce_core/juce_core.h>
#include <optional>

struct ProjectPatches {
    juce::File project;               // the project's folder, or the patches' own
    juce::Array<juce::File> patches;  // its patches, by file name
    std::optional<int> at;            // the patch asked about, among them
};

// The project `patch` is in, and its patches of `extension`, `at` the patch's place
ProjectPatches projectOf(const juce::File& patch, const juce::String& extension);
// A project's folder (one holding Patch, or holding the patches themselves) and its patches of `extension`
ProjectPatches projectAt(const juce::File& folder, const juce::String& extension);
// The project's name to show: its folder's, as the XT lists it
juce::String projectName(const ProjectPatches& p);
