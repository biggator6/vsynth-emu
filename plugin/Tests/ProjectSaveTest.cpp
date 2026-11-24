// A patch saved through the processor (ProjectTest.h, v602).  The image's patches applied one by one, each saved as it
// stands; then one value of each kind moved on one patch and the saved file read back by the firmware's reader.  The
// stores compared byte for byte, so a value that lands in the wrong place, or brings a neighbour with it, shows.
#include "ProjectTest.h"
#include "TestControls.h"
#include "TestPatches.h"
#include "../Source/PluginParamIds.h"
#include "../Source/PluginPatchSave.h"
#include "../Source/UiPatchSave.h"
#include "../Source/XtFiles.h"
#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace {
constexpr int kMfxType = 0x177, kCosmType = 0x3e8, kTvf = 10, kTvfCutoff = 0x924, kTvaLevel = 0x29a;
constexpr int kMfxMaster = 0x178, kFieldBytes = 4;   // a moved value's bytes in the store, at most (32-bit IDs)

// How many bytes of two stores differ
int storeBytesMoved(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    int n = 0;
    for (size_t i = 0; i < std::min(a.size(), b.size()); ++i) n += a[i] != b[i];
    return n + static_cast<int>(std::max(a.size(), b.size()) - std::min(a.size(), b.size()));
}

// The first-bank IDs two stores read otherwise, zone 1 and both COSMs, as "ID was→is", the first few
std::string idsMoved(const std::vector<uint8_t>& was, const std::vector<uint8_t>& is) {
    constexpr int kShown = 8;
    std::string out;
    int shown = 0;
    for (const XtPatchSectionDef& s : xtPatchSections())
        for (int id = s.first; id <= s.last && id < kXtDescIds && shown < kShown; ++id)
            for (int cosm = 0; cosm < (xtPatchIsCosm(id) ? 2 : 1); ++cosm) {
                const int a = xtPatchValue(was, id, 0, cosm), b = xtPatchValue(is, id, 0, cosm);
                if (a == b || shown >= kShown) continue;
                ++shown;
                out += juce::String::toHexString(id).toStdString() + (cosm ? "/2 " : " ") + std::to_string(a) + "→"
                       + std::to_string(b) + " ";
            }
    return out;
}

// The patch as the processor would save it now, read back by the firmware's reader
std::optional<XtPatchFile> savedNow(const VSynthEmuProcessor& p, const juce::String& name) {
    std::vector<uint8_t> file;
    XtPatchFile read;
    if (!patchFileOf(p, name, file) || !xtPatchRead(file.data(), file.size(), read)) return std::nullopt;
    return read;
}

void unchangedSaves(VSynthEmuProcessor& p, const std::vector<ImagePatch>& image, const juce::File& work,
                    TestChecks& check) {
    int same = 0;
    std::string first;
    for (const ImagePatch& m : image) {
        p.applyPatch(m.name, m.store, 0, work);
        std::vector<uint8_t> file;
        std::string why;
        if (patchFileOf(p, juce::String(m.name), file, &why) && file == m.file) {
            ++same;
            continue;
        }
        XtPatchFile read;
        const bool readable = xtPatchRead(file.data(), file.size(), read);
        const int moved = readable ? storeBytesMoved(read.store, m.store) : -1;
        const std::string ids = readable ? ", " + idsMoved(m.store, read.store) : std::string();
        if (first.empty())
            first = m.name + ": " + (why.empty() ? std::to_string(moved) + " bytes of its store moved" + ids : why);
    }
    check(same == (int)image.size(), std::to_string(same) + " of " + std::to_string(image.size())
                                         + " patches saved unchanged should be their files; the first not, " + first);
}

// One value moved on a patch, and where it is read back from the saved file
struct Moved {
    std::string what;
    std::function<void()> move;
    std::function<int(const std::vector<uint8_t>&)> read;
    int want = 0;
};

// The value read back as moved, and no byte past it moved
void movedLands(VSynthEmuProcessor& p, const ImagePatch& m, const juce::File& work, const Moved& v, TestChecks& check) {
    p.applyPatch(m.name, m.store, 0, work);
    v.move();
    const auto saved = savedNow(p, juce::String(m.name));
    const int read = saved ? v.read(saved->store) : -1, moved = saved ? storeBytesMoved(saved->store, m.store) : -1;
    check(saved && read == v.want && moved > 0 && moved <= kFieldBytes,
          v.what + " should land in the saved patch alone: read " + std::to_string(read) + ", wanted "
              + std::to_string(v.want) + "; " + std::to_string(moved) + " bytes of the store moved");
}

// A value in [lo, hi] other than `now`
int otherThan(int now, int lo, int hi) { return now < hi ? now + 1 : lo; }

void savesAs(const juce::File& work, TestChecks& check) {
    const juce::File folder = work.getChildFile("Slots.prj").getChildFile("Patch");
    for (const char* name : { "001 First.p00", "007 Seventh.p00", "Loose.p00" })
        writeTestFile(folder.getChildFile(name), { 0 });
    check(nextSlotFile(folder, "New", "p00") == folder.getChildFile("008 New.p00"),
          "Save As should offer the project's next slot past its highest");
    check(patchNameOfFile(folder.getChildFile("012 Bass.p00")) == "Bass"
              && patchNameOfFile(folder.getChildFile("Loose.p00")) == "Loose",
          "a saved patch should take its name from its file, past its slot");
}

// One of each kind: a tone's knob, an effect's knob, the MFX type's own parameter, COSM 2's typed cutoff
void eachKindMoved(VSynthEmuProcessor& p, const std::vector<ImagePatch>& image, const juce::File& work,
                   TestChecks& check) {
    const ImagePatch& m = image.front();
    const int level = otherThan(xtPatchValue(m.store, kTvaLevel), 0, 127);
    movedLands(p, m, work, { "the TVA's level", [&] { setParam(p, Params::kTvaLevel, (float)level); },
                             [](const auto& s) { return xtPatchValue(s, kTvaLevel); }, level }, check);
    const int master = otherThan(xtPatchValue(m.store, kMfxMaster), 0, 127);
    movedLands(p, m, work, { "the MFX's level", [&] { setParam(p, "fxMfxMaster", (float)master); },
                             [](const auto& s) { return xtPatchValue(s, kMfxMaster); }, master }, check);
    int first = 0, last = 0;
    if (xtEffectTypeIds(0, xtPatchValue(m.store, kMfxType), first, last) && xtTypedDesc(first)) {
        const XtTypedDesc& d = *xtTypedDesc(first);
        const int v = otherThan(xtPatchTyped(m.store, kXtMfxSlots, first), d.min, d.max);
        movedLands(p, m, work, { "the MFX type's first parameter", [&] { p.setFxParam(first, v); },
                                 [first](const auto& s) { return xtPatchTyped(s, kXtMfxSlots, first); }, v }, check);
    }
    const auto tvf = std::ranges::find_if(image, [](const ImagePatch& x) {
        return xtPatchValue(x.store, kCosmType, 0, 1) == kTvf;
    });
    check(tvf != image.end(), "a patch whose COSM 2 is a TVF should be in the image");
    if (tvf == image.end()) return;
    const int cutoff = otherThan(xtPatchTyped(tvf->store, kXtCosmSlots, kTvfCutoff, 0, 1), 0, 127);
    movedLands(p, *tvf, work, { "COSM 2's cutoff", [&] { setParam(p, Params::cosmId(1, "Cutoff"), (float)cutoff); },
                                [](const auto& s) { return xtPatchTyped(s, kXtCosmSlots, kTvfCutoff, 0, 1); },
                                cutoff }, check);
}
} // namespace

void projectSaveChecks(VSynthEmuProcessor& p, const juce::File& rom, const juce::File& work, TestChecks& check) {
    const std::vector<ImagePatch> image = imagePatches(rom);
    check(!image.empty(), "the image's patches should be there to save");
    if (image.empty()) return;
    unchangedSaves(p, image, work, check);
    eachKindMoved(p, image, work, check);
    p.applyPatch(image.front().name, image.front().store, 0, work);
    const auto named = savedNow(p, "Saved Name");
    check(named && named->name == "Saved Name", "a patch should save under the name it's given");
    savesAs(work, check);
}
