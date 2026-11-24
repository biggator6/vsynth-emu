// A project saved: see PluginProjectSave.h (v603).
#include "PluginProjectSave.h"
#include "PluginPatchFiles.h"
#include "PluginPatchSave.h"
#include "PluginProcessor.h"
#include "PluginProject.h"

using xtfile::fail;

namespace {
constexpr int kPcm = 1;   // [FW] OSC Type 1, PCM: an OSC that plays a wave
const juce::String kPatchFolder = "Patch", kWaveFolder = "Wave";

// `from`'s files of `pattern` (hidden ones passed over) into `to` as they are; how many, or none if one won't copy
std::optional<int> copyFiles(const juce::File& from, const juce::File& to, const juce::String& pattern) {
    juce::Array<juce::File> files;
    if (from.isDirectory())
        from.findChildFiles(files, juce::File::findFiles | juce::File::ignoreHiddenFiles, false, pattern);
    if (!to.createDirectory()) return std::nullopt;
    for (const juce::File& f : files)
        if (!f.copyFileTo(to.getChildFile(f.getFileName()))) return std::nullopt;
    return files.size();
}

// Each PCM OSC's wave from outside the project, copied in at the next free slot; a wave in the project's own folder at
// its own slot.  The slots each OSC's Wave Number takes; none (and why) if a wave won't copy
std::optional<PatchWaveSlots> wavesIn(const VSynthEmuProcessor& p, const juce::File& ownWaves, const juce::File& waves,
                                      ProjectSaved& report, std::string* why) {
    PatchWaveSlots slots;
    const XtSynth::Controls c = p.currentControls();
    for (size_t osc = 0; osc < slots.size(); ++osc) {
        const XtSynth::OscControls& o = osc ? c.osc2 : c;
        const juce::File wave = oscWaveFile(p, static_cast<int>(osc));
        if (o.oscType != kPcm || !wave.existsAsFile()) continue;
        if (ownWaves != juce::File() && wave.isAChildOf(ownWaves)) {
            slots[osc] = slotOfFile(wave);
            continue;
        }
        const juce::File to = nextSlotFile(waves, patchNameOfFile(wave), wave.getFileExtension().substring(1));
        if (!waves.createDirectory() || !wave.copyFileTo(to)) {
            fail(why, "a wave won't copy into the project");
            return std::nullopt;
        }
        ++report.waves;
        slots[osc] = slotOfFile(to);
        if (!isVSynthWave(wave)) report.toImport.add(to.getFileName());
    }
    return slots;
}
} // namespace

bool saveProject(const VSynthEmuProcessor& p, const juce::File& patch, const juce::File& dest, ProjectSaved& report,
                 std::string* why) {
    report = {};
    if (dest.exists()) return fail(why, "the project's folder is there already");
    if (!dest.createDirectory()) return fail(why, "the project's folder can't be made");
    const bool fromProject = patch.existsAsFile();
    const juce::File own = fromProject ? projectOf(patch, "p00").project : juce::File();
    const juce::File patches = dest.getChildFile(kPatchFolder), waves = dest.getChildFile(kWaveFolder);
    const juce::String name = p.getPatchName();
    if (fromProject) {
        const auto system = copyFiles(own, dest, "*.s00");
        const auto patchCount = copyFiles(patch.getParentDirectory(), patches, "*.p00");
        const auto waveCount = copyFiles(own.getChildFile(kWaveFolder), waves, "*");
        if (!system || !patchCount || !waveCount) return fail(why, "the project's files won't copy");
        report.patches = *patchCount;
        report.waves = *waveCount;
    }
    const auto slots = wavesIn(p, fromProject ? own.getChildFile(kWaveFolder) : juce::File(), waves, report, why);
    if (!slots) return false;
    report.patch = patches.getChildFile(fromProject ? patch.getFileName() : "001 " + name + ".p00");
    std::vector<uint8_t> bytes;
    if (!patches.createDirectory() || !patchFileOf(p, name, *slots, bytes, why)) return false;
    if (!report.patch.replaceWithData(bytes.data(), bytes.size())) return fail(why, "the patch can't be written");
    if (!fromProject) report.patches = 1;
    return true;
}
