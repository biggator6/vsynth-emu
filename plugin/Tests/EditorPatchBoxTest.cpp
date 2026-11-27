// The patch box's project (EditorTest.h, v601; research/UI_DESIGN.md, "The patch box"): a project of six of the image's
// patches written in `work` last slot first, with a hidden "._" file among them as a Mac leaves on the XT's drive.  With
// no patch browsed both arrows are grey; the first browsed (the session's property, as the box's own load sets it), ◀ is
// grey and the project's line says where; ▶ loads each next, to the last, where ▶ greys; ◀ loads the one before.  And
// the project's reads: its patches by slot, a patch outside a Patch folder browses its own folder, a project's folder
// finds its Patch folder.
#include "EditorTest.h"
#include "TestPatches.h"
#include "../Source/PluginProject.h"
#include "../Source/UiPatchBox.h"
#include <algorithm>
#include <string>
#include <vector>

namespace {
// Six: the Mac's enumerator lists a folder by its names' hashes (002, 001, 003 for three), so a listing left unsorted
// passes three by chance one time in six, six one time in 720
constexpr int kPatches = 6;
constexpr const char* kProject = "Box.prj";

// Where the box says the patch is: its project's line (shown when the patch misses nothing, its note there otherwise)
// and the arrows' tooltip
bool saysWhere(LayoutCheck& l, juce::Button& arrow, const juce::String& where) {
    const auto line = [&where](juce::Label& x) { return x.getText() == where; };
    return l.firstOf<juce::Label>(line) != nullptr && arrow.getTooltip() == where;
}

// The image's first patches of different names, as a project's Patch folder holds them, by slot; their names in
// `names`.  Written last slot first, so a listing in the folder's own order isn't by slot on any file system
std::vector<juce::File> writeProject(const juce::File& rom, const juce::File& project,
                                     std::vector<std::string>& names) {
    std::vector<juce::File> files;
    std::vector<const ImagePatch*> chosen;
    const std::vector<ImagePatch> image = imagePatches(rom);
    for (const ImagePatch& m : image) {
        if ((int)files.size() == kPatches) break;
        if (m.name.empty() || std::ranges::find(names, m.name) != names.end()) continue;
        const juce::String slot = juce::String((int)files.size() + 1).paddedLeft('0', 3);
        files.push_back(project.getChildFile("Patch").getChildFile(slot + " " + m.name + ".p00"));
        chosen.push_back(&m);
        names.push_back(m.name);
    }
    for (size_t k = files.size(); k-- > 0;) writeTestFile(files[k], chosen[k]->file);
    writeTestFile(project.getChildFile("Patch").getChildFile("._002 hidden.p00"), { 0, 5, 0, 7 });
    return files;
}
} // namespace

void patchBoxChecks(LayoutCheck& l, VSynthEmuProcessor& p, const juce::File& rom, const juce::File& work,
                    TestChecks& check) {
    PatchBox* box = l.firstOf<PatchBox>([](PatchBox&) { return true; });
    juce::Button* prev = l.buttonOf("previous patch");
    juce::Button* next = l.buttonOf("next patch");
    check(box && prev && next, "the patch box should have its arrows");
    if (!box || !prev || !next) return;
    p.apvts.state.removeProperty("patchFile", nullptr);
    box->refresh();
    check(!prev->isEnabled() && !next->isEnabled(), "with no patch browsed both arrows should be grey");

    std::vector<std::string> names;
    const juce::File project = work.getChildFile(kProject);
    const std::vector<juce::File> files = writeProject(rom, project, names);
    check((int)files.size() == kPatches, "the image should give enough patches of different names");
    if ((int)files.size() != kPatches) return;
    const ProjectPatches listed = projectOf(files[1], "p00");
    check(listed.project == project && listed.patches.size() == kPatches && listed.at == 1,
          "a patch's project should be its Patch folder's parent, its patches all there, the hidden one passed over");
    const bool bySlot = std::ranges::equal(files, listed.patches, [](const juce::File& a, const juce::File& b) {
        return a == b;
    });
    check(bySlot, "a project's patches should be listed by slot");
    check(projectAt(project, "p00").patches.size() == kPatches, "a project's folder should find its Patch folder");
    const ProjectPatches loose = projectOf(work.getChildFile("loose.p00"), "p00");
    check(loose.project == work && !loose.at, "a patch outside a Patch folder should browse its own folder");

    check(p.loadPatch(files[0]), "the project's first patch should load");
    p.apvts.state.setProperty("patchFile", files[0].getFullPathName(), nullptr);
    box->refresh();
    check(!prev->isEnabled() && next->isEnabled(), "at the first patch ◀ should be grey and ▶ not");
    const juce::String of = " of " + juce::String(kPatches);
    check(saysWhere(l, *next, juce::String(kProject) + ", 1" + of), "the box should say where the patch is");
    for (int k = 1; k < kPatches; ++k) {
        if (next->isEnabled()) next->onClick();
        check(p.getPatchName() == juce::String(names[(size_t)k]), "▶ should load patch " + std::to_string(k + 1));
    }
    check(prev->isEnabled() && !next->isEnabled(), "at the last patch ▶ should be grey and ◀ not");
    if (prev->isEnabled()) prev->onClick();
    check(p.getPatchName() == juce::String(names[kPatches - 2]), "◀ should load the patch before");
    check(saysWhere(l, *prev, juce::String(kProject) + ", " + juce::String(kPatches - 1) + of),
          "where the box says should follow the patch");
    check(box->initChoices() == juce::StringArray("Execute"), "the V-Synth's INIT should offer Execute alone");
    box->initialize(0);   // INIT's menu, its Execute (v473): the patch played initialized, the one browsed kept
    check(p.getPatchName() == "INIT PATCH", "INIT should give the PATCH Init, not " + p.getPatchName().toStdString());
    check(saysWhere(l, *prev, juce::String(kProject) + ", " + juce::String(kPatches - 1) + of),
          "INIT should leave the patch browsed as it was");
}
