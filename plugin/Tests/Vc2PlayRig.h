#pragma once
// VSynthEmuVc2PlayTest's set-up, its check's and its timing mode's (v455): the images and the VC-2's waves in, and a
// processor in the VC-2 mode with a .p02 loaded and its part made.
#include "TestChecks.h"
#include "TestPatches.h"
#include "../Source/PluginProcessor.h"
#include "../Source/Vc2WavesInUse.h"
#include "../Source/XtEncoded.h"
#include <cmath>
#include <functional>
#include <memory>
#include <string>

inline constexpr int kVc2PlayBlock = 512;        // the host's block
inline constexpr double kVc2WavesMost = 60.0;    // seconds the waves' load may take

// rom.2 (the part's ports read its tables), rom.4 beside it, and the waves from the disk image beside rom.4
inline void vc2PlayImages(TestChecks& check, const juce::File& rom) {
    const std::string rom4 = rom.getSiblingFile("V-SynthXT.rom.4.bin").getFullPathName().toStdString();
    check(xtSynthSetRom(rom.getFullPathName().toStdString()), "rom.2 should load: the part's ports read its tables");
    check(xtSynthSetVc2Rom(rom4), "rom.4 should load beside rom.2");
    const juce::File disk = rom.getSiblingFile("V-SynthXTDiskImage.bin.0.bin");
    check(vc2TakeWaves(disk.getFullPathName().toStdString(), rom4) && vc2WavesWait(kVc2WavesMost) && vc2Waves(),
          "the VC-2's waves should load from the disk image");
}

// A processor at `rate` in the VC-2 mode with `file` loaded, its part made
inline std::unique_ptr<VSynthEmuProcessor> vc2Processor(TestChecks& check, const juce::File& file, double rate) {
    auto p = std::make_unique<VSynthEmuProcessor>();
    p->fxManualTicks();
    p->prepareToPlay(rate, kVc2PlayBlock);
    setPluginMode(p->apvts.state, PluginMode::vc2);
    check(p->loadVc2Patch(file), file.getFileName().toStdString() + " should load");
    p->vc2Play().tick();
    check(p->vc2Play().player() != nullptr, "the VC-2 mode should make its part");
    return p;
}

// A processor at `rate` given a session's saved state as a host restores one, the control thread ticked once so its
// part is made (the system checks' session, the render mode's `state=`)
inline std::unique_ptr<VSynthEmuProcessor> vc2ProcessorFromState(const juce::MemoryBlock& state, double rate) {
    auto p = std::make_unique<VSynthEmuProcessor>();
    p->fxManualTicks();
    p->prepareToPlay(rate, kVc2PlayBlock);
    p->setStateInformation(state.getData(), (int)state.getSize());
    p->vc2Play().tick();
    return p;
}

// The disk image's .p02 at `place` (0 the first) written into `work` and loaded by vc2Processor at `rate`, its name in
// `name` (the timing and render modes'); none, and a failed check, where the image holds none
inline std::unique_ptr<VSynthEmuProcessor> vc2PlayPatchAt(TestChecks& check, const juce::File& rom,
                                                          const juce::File& work, int place, double rate,
                                                          std::string& name) {
    const std::vector<DiskPatch> patches = diskVc2Patches(rom);
    if (place < 0 || place >= (int)patches.size()) {
        check(false, "the disk image holds no .p02 at place " + std::to_string(place));
        return nullptr;
    }
    const DiskPatch& patch = patches[(size_t)place];
    const juce::File file = work.getChildFile(patch.name);
    writeTestFile(file, patch.bytes);
    name = patch.name;
    return vc2Processor(check, file, rate);
}

// The input the checks play: a saw at 220 Hz, a little higher on the right, at a quarter of full scale
inline float vc2PlayInput(int frame, int side) {
    constexpr double kPitch = 220.0, kRight = 1.01, kRate = 44100.0;
    constexpr float kLevel = 0.25f;
    const double cycle = frame * (kPitch * (side ? kRight : 1.0)) / kRate;
    return kLevel * (float)(2.0 * (cycle - std::floor(cycle)) - 1.0);
}

// The timing mode (Vc2PlayTime.cpp): `VSynthEmuVc2PlayTest <rom> time <place> [notes]`
int vc2PlayTime(const juce::File& rom, const juce::File& work, const juce::StringArray& rest);
// The render mode (Vc2PlayRender.cpp): `VSynthEmuVc2PlayTest <rom> render <place> <out.wav>`
int vc2PlayRender(const juce::File& rom, const juce::File& work, const juce::StringArray& rest);
// v456: the part's view as the editor reads it, on a processor that has played (Vc2PlayView.cpp)
void vc2PlayViewChecks(TestChecks& check, VSynthEmuProcessor& p, const std::string& name);
// v457: on a part just made, the system parameters at their defaults, rom.4's as the part reads them; on a processor that
// has played beside `direct`, the part reading its parameters' values and its readers' tables the direct part's, and a
// part made from its session reading the same (Vc2PlaySystem.cpp)
class Vc2PlayDirect;
// A part given rom.4's system defaults alone, nothing written over them: what its readers read is rom.4's system
std::unique_ptr<Vc2Synth> vc2PlayRom4System();
void vc2PlaySystemDefaults(TestChecks& check, VSynthEmuProcessor& p, const std::string& name);
void vc2PlaySystemChecks(TestChecks& check, VSynthEmuProcessor& p, const Vc2PlayDirect& direct,
                         const std::string& name);

// A run (Vc2PlayRun.cpp, v458 made one from v455's): kVc2PlayBlocks host blocks at 44.1 kHz of the host's MIDI (a chord,
// controllers, a bend, notes, a burst past the voices; CC 91 and 93 among them) with the saw on the input, against
// `direct` (none: nothing compared), `before(b)` ahead of each block b for its edits and the control thread's ticks
inline constexpr int kVc2PlayBlocks = 190;   // about 2.2 s
struct Vc2Played {
    long differ = 0, first = -1, allocations = 0;   // samples unlike the direct part's, the first's frame; processBlock's
    double peak = 0, sumSquares = 0, seconds = 0;   // seconds: processBlock's, all of them
    bool finite = true;
};
Vc2Played vc2PlayRun(VSynthEmuProcessor& p, Vc2PlayDirect* direct, const std::function<void(int)>& before);
// A parameter of the processor's moved, halfway down or, when that's where it stands, to the top; its value now
int vc2PlayMove(VSynthEmuProcessor& p, const juce::String& id);
// v458: the effects run, on `patch` (Vc2PlayFx.cpp)
struct DiskPatch;
void vc2PlayFxChecks(TestChecks& check, const DiskPatch& patch, const juce::File& work);
