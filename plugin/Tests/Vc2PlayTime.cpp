// VSynthEmuVc2PlayTest's timing mode (v455): `VSynthEmuVc2PlayTest <rom> time <place> [notes]`.  The disk image's .p02
// at `place` (0 the first) in a processor at 44.1 kHz in the VC-2 mode, `notes` notes from C4 up (one if none is said)
// held a second and let go a second, the input silent: what processBlock took against real time, the peak, and the
// part's blocks.  A measure, not in verify.sh (VC2_MODE_PLAN.md, step 4.5's [gap] on 018 FemaleChoirs).
#include "TestPatches.h"
#include "Vc2PlayRig.h"
#include <algorithm>
#include <chrono>
#include <cstdio>

namespace {
constexpr double kRate = 44100.0;
constexpr int kBlocksASecond = 86;   // host blocks of kVc2PlayBlock frames
constexpr int kC4 = 60;
constexpr juce::uint8 kVelocity = 100;
}

int vc2PlayTime(const juce::File& rom, const juce::File& work, const juce::StringArray& rest) {
    TestChecks check { "vc2play time" };
    vc2PlayImages(check, rom);
    const std::vector<DiskPatch> patches = diskVc2Patches(rom);
    const int place = rest[1].getIntValue(), notes = std::max(1, rest[2].getIntValue());
    if (place < 0 || place >= (int)patches.size()) {
        check(false, "the disk image holds no .p02 at place " + std::to_string(place));
        return check.summary();
    }
    const DiskPatch& patch = patches[(size_t)place];
    const juce::File file = work.getChildFile(patch.name);
    writeTestFile(file, patch.bytes);
    auto p = vc2Processor(check, file, kRate);
    juce::AudioBuffer<float> buf(2, kVc2PlayBlock);
    double seconds = 0, peak = 0;
    for (int b = 0; b < 2 * kBlocksASecond; ++b) {
        juce::MidiBuffer midi;
        for (int k = 0; k < notes; ++k) {
            if (b == 0) midi.addEvent(juce::MidiMessage::noteOn(1, kC4 + k, kVelocity), 0);
            if (b == kBlocksASecond) midi.addEvent(juce::MidiMessage::noteOff(1, kC4 + k), 0);
        }
        buf.clear();
        const auto began = std::chrono::steady_clock::now();
        p->processBlock(buf, midi);
        seconds += std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count();
        for (int ch = 0; ch < 2; ++ch) peak = std::max(peak, (double)buf.getMagnitude(ch, 0, kVc2PlayBlock));
    }
    const double audio = 2.0 * kBlocksASecond * kVc2PlayBlock / kRate;
    const Vc2PartPlayer::Counts& c = p->vc2Play().player()->counts();
    std::printf("vc2play time: %s, %d note(s): processBlock %.3f s for %.3f s of audio (%.0f%% of real time), peak "
                "%.4g; %ld blocks, %ld ahead in waits\n",
                patch.name.c_str(), notes, seconds, audio, 100.0 * seconds / audio, peak, c.blocks, c.ahead);
    return check.summary();
}
