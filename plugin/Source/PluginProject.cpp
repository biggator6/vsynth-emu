// A project's patches, to browse: see PluginProject.h (v601).
#include "PluginProject.h"
#include <algorithm>

namespace {
const juce::String kPatchFolder = "Patch";
constexpr int kSlotDigits = 3;   // the XT's "NNN name" files

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

std::optional<int> slotOfFile(const juce::File& file) {
    const juce::String n = file.getFileNameWithoutExtension();
    const juce::String digits = n.initialSectionContainingOnly("0123456789");
    if (digits.length() != kSlotDigits || n[kSlotDigits] != ' ') return std::nullopt;
    return digits.getIntValue();
}

juce::File nextSlotFile(const juce::File& folder, const juce::String& name, const juce::String& extension) {
    juce::Array<juce::File> files;   // any file holds its slot: a wave's .wav, .vpw or link alike (v603)
    if (folder.isDirectory())
        folder.findChildFiles(files, juce::File::findFiles | juce::File::ignoreHiddenFiles, false);
    int last = 0;
    for (const juce::File& f : files) last = std::max(last, slotOfFile(f).value_or(0));
    return folder.getChildFile(juce::String(last + 1).paddedLeft('0', kSlotDigits) + " " + name + "." + extension);
}

juce::String patchNameOfFile(const juce::File& file) {
    const juce::String n = file.getFileNameWithoutExtension();
    return slotOfFile(file) ? n.substring(kSlotDigits + 1) : n;
}
