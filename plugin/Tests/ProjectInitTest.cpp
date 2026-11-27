// The V-Synth's PATCH Init through the processor (ProjectTest.h, v473; PluginPatchInit.h).  The firmware's initialized
// patch is xtFwPatchInit's, which xt_files holds to the PATCH Init window's own EXECUTE run in the interpreter; here
// it must reach the processor as a loaded patch does, on a fresh processor and on one holding an image patch of several
// zones with a project's patch browsed: saved unchanged it is the init's store byte for byte, so no parameter reads
// otherwise than the firmware's store.
#include "ProjectTest.h"
#include "TestPatches.h"
#include "../Source/PluginPatchInit.h"
#include "../Source/XtArpeggioPatch.h"
#include "../Source/XtFwPatchInit.h"
#include "../Source/XtPatch.h"
#include <string>
#include <vector>

namespace {
constexpr const char* kInitName = "INIT PATCH";   // [FW] the name 0x89482540 gives a patch it names itself

// The image's first patch of more than one zone; null for none
const ImagePatch* severalZones(const std::vector<ImagePatch>& image) {
    for (const ImagePatch& m : image)
        if (xtPatchZones(m.store) > 1) return &m;
    return nullptr;
}

// The PATCH Init on `p`, and what it must give, said as `what`
void patchInitOn(VSynthEmuProcessor& p, const XtPatchFile& init, const std::string& what, TestChecks& check) {
    const XtSynth::Controls base = p.currentControls();
    const juce::var browsed = p.apvts.state["patchFile"];
    std::string why;
    check(initPatch(p, &why), what + ": INIT should run" + (why.empty() ? std::string() : ": " + why));
    check(p.getPatchName() == kInitName, what + ": named " + kInitName + ", not " + p.getPatchName().toStdString());
    check(p.patchZones() == xtPatchZones(init.store),
          what + ": the init's zones, not " + std::to_string(p.patchZones()));
    check(p.keptStore() == init.store, what + ": the init's store kept for the session");
    check(p.currentControls() == xtWithPatchArp(init.store, xtPatchControls(init.store, 0, base)),
          what + ": zone 1's controls and the arpeggio as the init's store gives them");
    const auto saved = savedNow(p, kInitName);
    const long moved = saved ? xtfile::storeBytesApart(saved->store, init.store) : -1;
    check(moved == 0, what + ": saved unchanged it should be the init's store, not " + std::to_string(moved)
                          + " bytes apart" + (saved ? ", " + idsMoved(init.store, saved->store) : std::string()));
    check(!p.hasFileLoaded(0) && !p.hasFileLoaded(1), what + ": no wave after the init");
    check(p.getPatchNote().isEmpty(), what + ": the init asks for nothing that isn't here, not "
                                          + p.getPatchNote().toStdString());
    check(p.apvts.state["patchFile"] == browsed, what + ": the patch browsed as it was");
}
} // namespace

void projectInitChecks(const juce::File& rom, const juce::File& work, TestChecks& check) {
    VSynthEmuProcessor fresh;
    check(fresh.setRomFile(rom), "the ROM should open for the init");
    XtPatchFile init;
    std::string why;
    check(xtFwPatchInit(init, &why), "the firmware's PATCH Init should run: " + why);
    patchInitOn(fresh, init, "a fresh processor", check);
    const std::vector<ImagePatch> image = imagePatches(rom);
    const ImagePatch* zoned = severalZones(image);
    check(zoned != nullptr, "the image should have a patch of several zones");
    if (zoned == nullptr) return;
    const juce::File file = work.getChildFile("Init.prj").getChildFile("Patch").getChildFile("001 Zoned.p00");
    auto p = loadedWith(check, rom, *zoned, file);
    p->apvts.state.setProperty("patchFile", file.getFullPathName(), nullptr);
    patchInitOn(*p, init, zoned->name + " (" + std::to_string(xtPatchZones(zoned->store)) + " zones)", check);
}
