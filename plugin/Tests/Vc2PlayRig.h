#pragma once
// VSynthEmuVc2PlayTest's set-up, its check's and its timing mode's (v455): the images and the VC-2's waves in, and a
// processor in the VC-2 mode with a .p02 loaded and its part made.
#include "TestChecks.h"
#include "../Source/PluginProcessor.h"
#include "../Source/Vc2WavesInUse.h"
#include "../Source/XtEncoded.h"
#include <cmath>
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

// The input the checks play: a saw at 220 Hz, a little higher on the right, at a quarter of full scale
inline float vc2PlayInput(int frame, int side) {
    constexpr double kPitch = 220.0, kRight = 1.01, kRate = 44100.0;
    constexpr float kLevel = 0.25f;
    const double cycle = frame * (kPitch * (side ? kRight : 1.0)) / kRate;
    return kLevel * (float)(2.0 * (cycle - std::floor(cycle)) - 1.0);
}

// The timing mode (Vc2PlayTime.cpp): `VSynthEmuVc2PlayTest <rom> time <place> [notes]`
int vc2PlayTime(const juce::File& rom, const juce::File& work, const juce::StringArray& rest);
// v456: the part's view as the editor reads it, on a processor that has played (Vc2PlayView.cpp)
void vc2PlayViewChecks(TestChecks& check, VSynthEmuProcessor& p, const std::string& name);
