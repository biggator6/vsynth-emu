// The firmware's tables are the process's, and every instance plays from them (R7 of .claude/claude-packs-review.md).
// One instance is given another image while the others play: what they play and the image their firmware calls read
// stay as they were, and the one given it is told why not.  A copy of the image in use, at another path, is taken
// as the image it is, with nothing read into the tables again.  The other image here is the ROM with one entry of the
// envelopes' time table a millisecond longer: a different image that loads, whose difference a note can hear.
// Run: VSynthEmuRomTest <rom.2.bin>.  Exit 0 if every check holds.
#include "TestAppDir.h"
#include "TestChecks.h"
#include "TestControls.h"
#include "../Source/PluginProcessor.h"
#include "../Source/XtFirmware.h"
#include <cstdint>
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
    p.prepareToPlay(48000.0, kBlock);
    juce::AudioBuffer<float> buf(2, kBlock);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100), 0);
    std::vector<float> out;
    for (int b = 0; b < kBlocks; ++b) {
        buf.clear();
        p.processBlock(buf, midi);
        midi.clear();
        out.insert(out.end(), buf.getReadPointer(0), buf.getReadPointer(0) + kBlock);
    }
    return out;
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

int romTest(const juce::File& rom, const juce::File& work) {
    TestChecks check { "rom" };
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

    { VSynthEmuProcessor p;
      const bool took = p.setRomFile(other);
      const juce::String said = p.getStatusText();
      check(!took && said.contains(rom.getFileName()) && said.contains("restart"),
            "another image should be refused while one is in use, and say why: \"" + said.toStdString() + "\""); }
    check(xtFwRomPath() == inUse, "another image shouldn't reach the firmware's calls: they read " + xtFwRomPath());
    check(playing.hasRom(), "the instance playing should keep its image");
    check(attackNote() == before, "a note should sound as before another instance was given another image");
    return check.summary();
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s rom.2.bin\n", argc > 0 ? argv[0] : "");
        return 1;
    }
    juce::ScopedJuceInitialiser_GUI init;
    TestAppDir appDir;
    const juce::File work = appDir.dir.getChildFile("rom");
    work.createDirectory();
    return romTest(juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]), work);
}
