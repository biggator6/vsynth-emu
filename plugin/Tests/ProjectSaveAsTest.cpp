// A project saved through the processor (ProjectTest.h, v603), on the link test's project (its patch loaded, its slot
// linked to a preset wave), with a second patch and a system file put beside them.  Saved unchanged, the new project
// holds the same files byte for byte; its folder there already is refused; a plain WAV loaded into OSC 2 comes in at
// the next slot, named for the XT to import, the saved patch's OSC 2 on it; a patch from no project saves alone.
#include "ProjectTest.h"
#include "TestControls.h"
#include "TestPatches.h"
#include "TestWave.h"
#include "../Source/PluginParamIds.h"
#include "../Source/PluginProject.h"
#include "../Source/PluginProjectSave.h"
#include "../Source/XtPatch.h"
#include <algorithm>
#include <string>
#include <vector>

namespace {
// Every file under `a` is under `b` at the same place, byte for byte, and no more: empty if so, else the first that
// isn't
juce::String filesApart(const juce::File& a, const juce::File& b) {
    juce::Array<juce::File> files, others;
    a.findChildFiles(files, juce::File::findFiles | juce::File::ignoreHiddenFiles, true);
    b.findChildFiles(others, juce::File::findFiles | juce::File::ignoreHiddenFiles, true);
    if (files.size() != others.size() || files.isEmpty())
        return juce::String(files.size()) + " files against " + juce::String(others.size());
    const auto differs = std::ranges::find_if(files, [&a, &b](const juce::File& f) {
        return !f.hasIdenticalContentTo(b.getChildFile(f.getRelativePathFrom(a)));
    });
    return differs == files.end() ? juce::String() : differs->getRelativePathFrom(a);
}

// A patch file's store, empty if it isn't one
std::vector<uint8_t> storeOf(const juce::File& patch) {
    juce::MemoryBlock bytes;
    XtPatchFile read;
    if (!patch.loadFileAsData(bytes)
        || !xtPatchRead(static_cast<const uint8_t*>(bytes.getData()), bytes.getSize(), read))
        return {};
    return read.store;
}

// The OSC 2 Wave Number of the patch a project saved
int savedOsc2Wave(const juce::File& patch) {
    const std::vector<uint8_t> store = storeOf(patch);
    return store.empty() ? -1 : xtPatchWaveNumber(store, 0, 1);
}

// The link test's project's one wave, a link, and its slot
int linkSlot(const juce::File& project) {
    juce::Array<juce::File> waves;
    project.getChildFile("Wave").findChildFiles(waves, juce::File::findFiles | juce::File::ignoreHiddenFiles, false);
    return waves.size() == 1 ? slotOfFile(waves.getFirst()).value_or(-1) : -1;
}
} // namespace

void projectSaveAsChecks(VSynthEmuProcessor& p, const juce::File& patch, const juce::File& rom,
                         const juce::File& work, TestChecks& check) {
    const juce::File own = projectOf(patch, "p00").project;
    const std::vector<ImagePatch> image = imagePatches(rom);
    check(!image.empty() && linkSlot(own) > 0, "the link test's project should be there, one link in its Wave folder");
    if (image.empty() || linkSlot(own) <= 0) return;
    writeTestFile(own.getChildFile("Patch").getChildFile("002 Second.p00"), image.back().file);
    writeTestFile(own.getChildFile("V-Synth System.s00"), { 1, 2, 3 });

    ProjectSaved r;
    std::string why;
    const juce::File same = work.getChildFile("Same.prj");
    check(saveProject(p, patch, same, r, &why), "a project should save: " + why);
    const juce::String apart = filesApart(own, same);
    const juce::File savedPatch = same.getChildFile(patch.getRelativePathFrom(own));
    check(apart.isEmpty(), "a project saved unchanged should hold its files byte for byte: " + apart.toStdString()
                               + "; " + idsMoved(storeOf(patch), storeOf(savedPatch)));
    check(r.patches == 2 && r.waves == 1 && r.toImport.isEmpty()
              && r.patch == same.getChildFile("Patch").getChildFile(patch.getFileName()),
          "the save should say 2 patches and 1 wave, none to import, its patch in its slot");
    check(!saveProject(p, patch, same, r, &why), "a project's folder that's there already should be refused");

    setParam(p, Params::oscId(1, Params::kOscType), 1.0f);   // OSC 2 PCM, a plain WAV from outside on it
    check(work.getChildFile("Outside").createDirectory(), "the outside wave's folder should be made");
    const juce::File plain = writeTestWave(work.getChildFile("Outside"), 1);
    p.loadFile(plain, 1);
    const juce::File added = work.getChildFile("Added.prj");
    check(saveProject(p, patch, added, r, &why), "a project with an outside wave should save: " + why);
    const int slot = linkSlot(own) + 1;
    const juce::File in = added.getChildFile("Wave").getChildFile(
        juce::String(slot).paddedLeft('0', 3) + " " + patchNameOfFile(plain) + ".wav");
    check(in.hasIdenticalContentTo(plain) && r.toImport.contains(in.getFileName()),
          "an outside plain WAV should come in at the next slot as it is, named for the XT to import: "
              + std::to_string(r.waves) + " waves, to import " + r.toImport.joinIntoString(", ").toStdString()
              + "; OSC 2 holds " + p.apvts.state["loadedFilePath2"].toString().toStdString() + ", type "
              + std::to_string(p.currentControls().osc2.oscType));
    check(savedOsc2Wave(r.patch) == slot, "the saved patch's OSC 2 should play the outside wave's slot, "
                                              + std::to_string(slot) + ": " + std::to_string(savedOsc2Wave(r.patch)));

    VSynthEmuProcessor fresh;   // a patch from no project
    fresh.setRomFile(rom);
    fresh.applyPatch(image.front().name, image.front().store, 0, work);
    const juce::File alone = work.getChildFile("Alone.prj");
    const bool saved = saveProject(fresh, juce::File(), alone, r, &why);
    check(saved && r.patches == 1 && r.patch.existsAsFile()
              && r.patch == alone.getChildFile("Patch").getChildFile("001 " + image.front().name + ".p00"),
          "a patch from no project should save alone at slot 001: " + why);
}
