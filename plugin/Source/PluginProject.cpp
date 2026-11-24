// A project's patches, to browse: see PluginProject.h (v601).
#include "PluginProject.h"

namespace {
const juce::String kPatchFolder = "Patch";

// A folder's patches of `extension`, by file name (hidden ones, as a Mac leaves "._" files on the XT's drive, passed over)
juce::Array<juce::File> patchesIn(const juce::File& folder, const juce::String& extension) {
    juce::Array<juce::File> out;
    if (folder.isDirectory())
        folder.findChildFiles(out, juce::File::findFiles | juce::File::ignoreHiddenFiles, false, "*." + extension);
    out.sort();
    return out;
}
} // namespace

ProjectPatches projectOf(const juce::File& patch, const juce::String& extension) {
    const juce::File folder = patch.getParentDirectory();
    const bool inProject = folder.getFileName().equalsIgnoreCase(kPatchFolder);
    ProjectPatches p { inProject ? folder.getParentDirectory() : folder, patchesIn(folder, extension), std::nullopt };
    if (const int i = p.patches.indexOf(patch); i >= 0) p.at = i;
    return p;
}

ProjectPatches projectAt(const juce::File& folder, const juce::String& extension) {
    const juce::File inside = folder.getChildFile(kPatchFolder);
    return { folder, patchesIn(inside.isDirectory() ? inside : folder, extension), std::nullopt };
}

juce::String projectName(const ProjectPatches& p) { return p.project.getFileName(); }
