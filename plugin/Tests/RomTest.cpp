// The firmware's tables are the process's, and every instance plays from them (R7 of .claude/claude-packs-review.md).
// One instance is given another image while the others play: what they play and the image their firmware calls read
// stay as they were, and the one given it is told why not.  A copy of the image in use, at another path, is taken
// as the image it is, with nothing read into the tables again.  The other image here is the ROM with one entry of the
// envelopes' time table a millisecond longer: a different image that loads, whose difference a note can hear.
// Run: VSynthEmuRomTest <rom.2.bin>.  Exit 0 if every check holds.
#include "TestChecks.h"
#include "TestControls.h"
#include "TestMain.h"
#include "TestNote.h"
#include "../Source/PluginAppFolder.h"
#include "../Source/PluginProcessor.h"
#include "../Source/Vc2CarrierTables.h"
#include "../Source/Vc2CosmTables.h"
#include "../Source/Vc2MatrixTables.h"
#include "../Source/Vc2PartVoicesTables.h"
#include "../Source/Vc2KeysTables.h"
#include "../Source/Vc2SetEnvelopeTables.h"
#include "../Source/Vc2PartSettingsTables.h"
#include "../Source/Vc2OsMidiTables.h"
#include "../Source/Vc2HiresoTables.h"
#include "../Source/Vc2LegacyTables.h"
#include "../Source/Vc2Params.h"
#include "../Source/Vc2TrackerTables.h"
#include "../Source/Vc2Kind6Tables.h"
#include "../Source/Vc2LibMathTables.h"
#include "../Source/Vc2AutoNoteTables.h"
#include "../Source/Vc2OscTables.h"
#include "../Source/Vc2OscConvertTables.h"
#include "../Source/Vc2RampTables.h"
#include "../Source/Vc2ToneMixTables.h"
#include "../Source/Vc2TvaTables.h"
#include "../Source/Vc2PartMixerTables.h"
#include "../Source/Vc2Unvoiced.h"
#include "../Source/Vc2VocoderTables.h"
#include "../Source/XtFirmware.h"
#include <cstdint>
#include <cstdio>
#include <utility>
#include <vector>

namespace {

constexpr uint32_t kRomBase = 0x89300000;   // [FW] where rom.2 maps
constexpr uint32_t kTimeTab = 0x897cec24;   // [FW] the envelopes' time by knob, u16 x 128 (XtEnvelope.cpp's kTimeTab)
constexpr int kAttack = 30;                 // the OSC Env Attack the note plays, its time that table's entry
constexpr int kBlock = 512, kBlocks = 64;   // 0.68 s at 48 kHz, inside the attack

// A note on the analog OSC with its attack at kAttack, from a new instance: what any instance plays from the tables
std::vector<float> attackNote() {
    VSynthEmuProcessor p;
    setParam(p, "oscType", 0.0f);   // ANALOG: no wave to encode
    setParam(p, "oscEnvAttack", (float)kAttack);
    return playNote(p, kBlock, kBlocks, 1);
}

// The ROM with the time at kAttack one longer; an empty File if that wouldn't still rise as the table must
juce::File otherImage(const juce::File& rom, const juce::File& work) {
    juce::MemoryBlock bytes;
    if (!rom.loadFileAsData(bytes)) return {};
    const size_t at = kTimeTab - kRomBase + 2u * kAttack;
    if (at + 4 > bytes.getSize()) return {};
    auto* b = static_cast<uint8_t*>(bytes.getData());
    const int t = b[at] | b[at + 1] << 8, next = b[at + 2] | b[at + 3] << 8;
    if (t + 1 >= next) return {};
    b[at] = (uint8_t)((t + 1) & 0xff);
    b[at + 1] = (uint8_t)((t + 1) >> 8);
    const juce::File other = work.getChildFile("another image.bin");
    return other.replaceWithData(bytes.getData(), bytes.getSize()) ? other : juce::File();
}

// rom.4's tables, which its loader reads at the start: the parameters' (v199), each bank's last ID, the last name and a
// value's text (Vc2Params.h), the unvoiced detector's coefficients (v208, Vc2Unvoiced.h), LEGACY's tables (v210,
// Vc2LegacyTables.h), HIRESO's (v213, Vc2HiresoTables.h), the vocoder object's (v220, Vc2VocoderTables.h) and the
// later ports' (the carrier's, the tracker's, kind 6's, Auto Note's, the part mixer's, v221–v227, the OSC voice's,
// v230, the TVA's, v233, COSM's, v234, the Matrix's, v235, the pitch ramp's, v236, the OSC block's converter's, v251,
// the library's logf and powf, the tone's OSC mix's, v269, the part's voices', v272, the part's keys', v276, C's
// envelopes', v282, the part's settings', v287, the OS MIDI task's, v313); what they say is each port's check's to check
bool vc2TablesIn() {
    return vc2ParamDesc(kVc2FirstTyped - 1) && vc2TypedDesc(kVc2Ids - 1) && !vc2ParamName(kVc2Ids - 1).empty()
           && !vc2ValueText(0, 0).empty() && vc2UnvoicedLoaded() && vc2LegacyLoaded() && vc2HiresoLoaded()
           && vc2VocoderLoaded() && vc2CarrierLoaded() && vc2TrackerLoaded() && vc2Kind6Loaded() && vc2AutoNoteLoaded()
           && vc2MixerLoaded() && vc2OscLoaded() && vc2TvaLoaded() && vc2CosmLoaded() && vc2MatrixLoaded()
           && vc2RampLoaded() && vc2OscConvertLoaded() && vc2LibMathLoaded() && vc2ToneMixLoaded()
           && vc2PartVoicesLoaded() && vc2KeysLoaded() && vc2SetEnvelopesLoaded() && vc2PartSettingsLoaded()
           && vc2OsMidiLoaded();
}

// An instance given `file` by `choose` should refuse it, its status saying `says`
void refuses(TestChecks& check, bool (VSynthEmuProcessor::*choose)(const juce::File&), const juce::File& file,
             const juce::String& says, const std::string& what) {
    VSynthEmuProcessor p;
    const bool took = (p.*choose)(file);
    const juce::String said = p.getStatusText();
    check(!took && said.contains(says), what + ": \"" + said.toStdString() + "\"");
}

// The two known images (v198): rom.4 found beside the rom.2 an instance opened, as the firmware update's unpacker leaves
// them; each refused as the other's, saying whose it is; a file of neither refused as no image this plugin knows; and
// none of them reaching the firmware's calls
void twoImages(TestChecks& check, const juce::File& rom, const juce::File& work, const std::string& inUse) {
    const juce::File vc2 = rom.getSiblingFile("V-SynthXT.rom.4.bin"), neither = work.getChildFile("not an image.bin");
    check(vc2.existsAsFile() && neither.replaceWithText("not a firmware image"),
          "rom.4 should be beside rom.2, and the test's file written: " + vc2.getFullPathName().toStdString());
    check(VSynthEmuProcessor().hasVc2Rom(), "rom.4 should be found beside the rom.2 an instance opened");
    check(vc2TablesIn(),
          "rom.4's tables (v199's parameters, v208's detector, LEGACY's, HIRESO's, the vocoder's, the carrier's, the "
          "tracker's, kind 6's, Auto Note's) should be in once rom.4 is");
    refuses(check, &VSynthEmuProcessor::setRomFile, vc2, "the VC-2's image, rom.4",
            "rom.4 should be refused as rom.2, saying whose it is");
    refuses(check, &VSynthEmuProcessor::setVc2RomFile, rom, "the V-Synth's image, rom.2",
            "rom.2 should be refused as rom.4, saying whose it is");
    refuses(check, &VSynthEmuProcessor::setRomFile, neither, "isn't a firmware image this plugin knows",
            "a file of neither image should be refused");
    check(xtFwRomPath() == inUse, "the other images shouldn't reach the firmware's calls: they read " + xtFwRomPath());
}

// rom.4 sought at every start, with no rom.2 to find (v198; the user's "load both roms so that switching modes is
// seamless"): a fresh run of this program, in an app folder holding rom.4 alone, has rom.4 and no rom.2.  Run before
// this process holds either image, and its link taken out after, so the checks below still find rom.4 beside rom.2.
void vc2Alone(TestChecks& check, const juce::File& rom) {
    const juce::File there = appDir().getChildFile("V-SynthXT.rom.4.bin");
    check(rom.getSiblingFile("V-SynthXT.rom.4.bin").createSymbolicLink(there, true),
          "can't link rom.4 in the app folder");
    const juce::String said = thisProgramSays({ rom.getFullPathName(), "--alone" });
    there.deleteFile();
    check(said.contains("rom.2 no, rom.4 yes, its tables in"),
          "rom.4 should be found at a start with no rom.2: \"" + said.trim().toStdString() + "\"");
}

int romTest(const juce::File& rom, const juce::File& work) {
    TestChecks check { "rom" };
    vc2Alone(check, rom);
    VSynthEmuProcessor playing;
    check(playing.setRomFile(rom), "the ROM should load");
    const std::string inUse = xtFwRomPath();
    const auto before = attackNote();
    const juce::File same = work.getChildFile("the same image.bin");
    const juce::File other = otherImage(rom, work);
    check(rom.copyFileTo(same) && other.existsAsFile(), "can't write the test's images");

    { VSynthEmuProcessor p;
      check(p.setRomFile(same) && p.getStatusText() == "ROM ready",
            "a copy of the image in use should be taken: \"" + p.getStatusText().toStdString() + "\""); }
    check(xtFwRomPath() == inUse, "a copy shouldn't be read into the tables again: the calls read " + xtFwRomPath());

    refuses(check, &VSynthEmuProcessor::setRomFile, other, rom.getFileName() + "; restart",
            "another image should be refused while one is in use, and say why");
    check(xtFwRomPath() == inUse, "another image shouldn't reach the firmware's calls: they read " + xtFwRomPath());
    check(playing.hasRom(), "the instance playing should keep its image");
    check(attackNote() == before, "a note should sound as before another instance was given another image");
    twoImages(check, rom, work, inUse);
    check(attackNote() == before, "a note should sound as before the other images were offered");
    return check.summary();
}

} // namespace

int main(int argc, char** argv) {
    if (argc == 3 && juce::String(argv[2]) == "--alone") {   // the child, in the parent's app folder (vc2Alone)
        juce::ScopedJuceInitialiser_GUI init;
        VSynthEmuProcessor p;
        std::printf("rom.2 %s, rom.4 %s, its tables %s\n", p.hasRom() ? "yes" : "no", p.hasVc2Rom() ? "yes" : "no",
                    vc2TablesIn() ? "in" : "out");
        return 0;
    }
    return testMain(argc, argv, "rom", romTest);
}
