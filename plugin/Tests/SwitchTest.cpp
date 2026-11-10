// The research switches a harness reads from its environment, set in the shell a host is started from (R8 of
// .claude/claude-packs-review.md): XT_ROUND=nearest, the encoder's traces and dumps, the firmware's prints and the
// earlier engine's trace.  The plugin reads none of them, so a SOLO note and a BACKING note sound as they do without
// them, nothing more is said and no file is written.  Each run is a process of its own (this program again, with
// --play), as a switch can be read once, when a process first needs it.
// Run: VSynthEmuSwitchTest <rom.2.bin>.  Exit 0 if every check holds.
#include "TestChecks.h"
#include "TestMain.h"
#include "TestNote.h"
#include "TestWave.h"
#include "../Source/PluginProcessor.h"
#include "../Source/XtFiles.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

constexpr int kBlock = 512, kBlocks = 64;          // 0.68 s at 48 kHz
constexpr float kSolo = 1.0f, kBacking = 2.0f;     // the Encode Type's choice index: 0 Auto, 1 SOLO, 2 BACKING
constexpr float kHeard = 0.001f;                   // a peak a note is heard at, so two silences can't pass as equal

// A switch and what the test sets it to: a file's name in the work folder, or the text itself
struct Switch {
    const char* name;
    const char* value;
    bool file;
};
constexpr Switch kSwitches[] = {
    { "XT_ROUND", "nearest", false },  { "XT_TRACE", "trace", true },    { "XT_DUMPDEC", "dec", true },
    { "XT_DUMPFIR", "fir", true },     { "XT_FW_TRACE", "1", false },    { "VSE_TRACE", "vse trace", true },
};

// One note on the wave encoded as `type`: "<digest> <peak>" of what it plays, or why it can't
std::string note(const juce::File& rom, const juce::File& wave, float type) {
    VSynthEmuProcessor p;
    if (!p.setRomFile(rom)) return "the ROM wasn't taken";
    const juce::String status = encodeWave(p, wave, type);
    if (!status.startsWith("Encoded")) return "not encoded: " + status.toStdString();
    const std::vector<float> out = playNote(p, kBlock, kBlocks, 2);
    float peak = 0.0f;
    for (const float s : out) peak = std::max(peak, std::fabs(s));
    char said[64];
    std::snprintf(said, sizeof said, "%016llx %.6f", (unsigned long long)xtfile::fnv(out.data(), out.size() * 4), peak);
    return said;
}

// The child's part: both notes, a line each
int play(const juce::File& rom, const juce::File& work) {
    const juce::File wave = work.getChildFile("wave_2s.wav");
    std::printf("SOLO %s\n", note(rom, wave, kSolo).c_str());
    std::printf("BACKING %s\n", note(rom, wave, kBacking).c_str());
    return 0;
}

// This program again, playing both notes in the environment as it now is: all it says, stdout and stderr
juce::String played(const juce::File& rom, const juce::File& work) {
    juce::ChildProcess child;
    const juce::StringArray args { juce::File::getSpecialLocation(juce::File::currentExecutableFile).getFullPathName(),
                                   rom.getFullPathName(), "--play", work.getFullPathName() };
    if (!child.start(args, juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr)) return "didn't start";
    const juce::String said = child.readAllProcessOutput();
    child.waitForProcessToFinish(120000);
    return said;
}

// The work folder's files, by name
juce::StringArray filesIn(const juce::File& work) {
    juce::StringArray names;
    for (const auto& f : work.findChildFiles(juce::File::findFiles, false)) names.add(f.getFileName());
    names.sort(false);
    return names;
}

// A note line's peak, if the note played
bool heard(const juce::String& line) {
    return line.fromLastOccurrenceOf(" ", false, false).getFloatValue() > kHeard;
}

int switchTest(const juce::File& rom, const juce::File& work) {
    TestChecks check { "switches" };
    if (writeTestWave(work, 2) == juce::File()) {
        std::fprintf(stderr, "switches: no test wave\n");
        return 1;
    }
    const juce::String plain = played(rom, work);
    const juce::StringArray lines = juce::StringArray::fromLines(plain.trim());
    check(lines.size() == 2 && lines[0].startsWith("SOLO ") && lines[1].startsWith("BACKING ") && heard(lines[0])
              && heard(lines[1]),
          "without the switches, both notes should play: " + plain.toStdString());
    const juce::StringArray filesBefore = filesIn(work);

    for (const Switch& s : kSwitches)
        setenv(s.name, s.file ? work.getChildFile(s.value).getFullPathName().toRawUTF8() : s.value, 1);
    const juce::String switched = played(rom, work);
    for (const Switch& s : kSwitches) unsetenv(s.name);
    check(switched == plain, "with the switches set, the notes should sound as without them, and nothing more be "
                             "said: \n" + switched.toStdString());
    check(filesIn(work) == filesBefore, "the switches shouldn't write files: "
                                            + filesIn(work).joinIntoString(", ").toStdString());
    return check.summary();
}

} // namespace

int main(int argc, char** argv) {
    if (argc == 4 && juce::String(argv[2]) == "--play") {   // the child, in the parent's app folder
        juce::ScopedJuceInitialiser_GUI init;
        const juce::File rom = juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]);
        return play(rom, juce::File(juce::String(argv[3])));
    }
    return testMain(argc, argv, "switches", switchTest);
}
