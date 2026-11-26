// The VC-2 mode's audio (v455; PluginVc2Play.h, Vc2PartPlayer.h): the processor with the mode switch on the VC-2 plays its
// part, held bit for bit to the part driven directly (Vc2PlayDirect.h), as PluginTest holds the V-Synth to XtSynth.  For a
// few of the disk image's .p02s, each loaded into a fresh processor at 44.1 kHz: a chord and single notes through the
// host's MIDI at frames off the blocks' edges, controllers and a bend, a burst of overlapping notes past the voices, the
// input a voice-like saw on both sides (Vc2PlayRun.cpp), a VC-2 parameter moved halfway, and (v457) three system
// parameters moved in the burst (Vc2PlaySystem.cpp's checks after); every sample out the same as the part's own (the
// processor's output passing OutputFinite, a non-number 0 on both sides), nothing allocated or freed in processBlock.
// Then: the session keeps the mode and the store the part played; the V-Synth mode makes no part; at 48 kHz (the first
// patch) the part is heard through the rate's conversion, finite, near the same level; rom.4's system defaults in, Part 1
// Receive Switch on and USB-MIDI Thru off, and no parameter holding either.  v458: with the Effects switch on, the part
// through rom.4's effects (Vc2PlayFx.cpp).  Run: VSynthEmuVc2PlayTest <rom.2.bin>, rom.4 and the disk image beside it.
// Exit 0 if every check holds.
#include "TestChecks.h"
#include "TestMain.h"
#include "TestPatches.h"
#include "Vc2PlayDirect.h"
#include "Vc2PlayRig.h"
#include "../Source/Vc2PatchFile.h"
#include "../Source/Vc2Params.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr double kRate = 44100.0, kOtherRate = 48000.0;
constexpr int kBlock = kVc2PlayBlock, kBlocks = kVc2PlayBlocks, kEditAt = 100;   // the parameter moved at the 100th
// v457: the system's moved at the 150th, in the burst (Vc2PlaySystem.cpp): Clock Source and External In Type, system B's,
// and Knob 1 Assign, the system's
constexpr int kSystemAt = 150;
constexpr int kSystemMoved[] = { 0x43, 0x7a, 0x95 };
// The disk image's .p02s played, by place, the first at 48 kHz as well.  v460: 018 FemaleChoirs back, left out before
// v362: one note-on's SOLO seek walked records that all read 0 (vc2SeekRecords through the tones' boundFloat answering 0
// for an address in no tone, 1e9 steps), nine times real time and nearly silent; with v362's boundFloat it plays
constexpr size_t kPatchesPlayed[] = { 0, 17, 40, 101, 120 };
constexpr size_t kFxPatch = 40;                              // v458: the one played through the effects
constexpr int kReceiveSwitch = 0x47, kUsbThru = 0x7d;        // [FW] Part 1 Receive Switch, USB-MIDI Thru: system B's
constexpr double kLevelNear = 6.0;                           // dB the 48 kHz run's level may stand from 44.1 kHz's

// A VC-2 parameter of zone 1 named as a level, its value moved: the first whose name says Level
const Vc2HostParam* levelParam() {
    for (const Vc2HostParam& h : vc2HostParams()) {
        const bool stored = h.zone == 0 && h.cosm == 0 && h.id >= kVc2FirstStored;
        if (stored && vc2ParamName(h.id).find("Level") != std::string::npos) return &h;
    }
    return nullptr;
}

// The processor's blocks against the direct part's at 44.1 kHz (none at another rate): a level moved at the 100th, the
// system at the 150th, the control thread ticked at each
Vc2Played play(VSynthEmuProcessor& p, Vc2PlayDirect* direct) {
    const Vc2HostParam* h = levelParam();
    return vc2PlayRun(p, direct, [&](int b) {
        if (b == kEditAt && h != nullptr) {   // a level moved: the processor's parameter, the direct part's store
            const int v = vc2PlayMove(p, h->paramId);
            p.vc2Play().tick();
            if (direct != nullptr) direct->edit(*h, v);
        }
        if (b == kSystemAt) {   // v457: system parameters moved, mid-burst: the processor's, the direct part's system
            std::vector<std::pair<int, int>> moves;
            for (const int id : kSystemMoved) moves.push_back({ id, vc2PlayMove(p, vc2HostParamId(id, 0, 0)) });
            p.vc2Play().tick();
            if (direct != nullptr) direct->systemEdit(moves);
        }
    });
}

// A run's mean power in dB, from its sum of squares over every frame played
double powerDb(double sumSquares) {
    return 10.0 * std::log10(std::max(sumSquares / ((double)kBlocks * kBlock), 1e-30));
}

// At 48 kHz: the part through the rate's conversion, finite, near the level the 44.1 kHz run `r` had
void otherRate(TestChecks& check, const juce::File& file, const Vc2Played& r, const std::string& name) {
    auto other = vc2Processor(check, file, kOtherRate);
    const Vc2Played o = play(*other, nullptr);
    check(o.finite && o.allocations == 0, name + "at 48 kHz: not finite, or allocations in processBlock");
    const double off = std::fabs(powerDb(o.sumSquares) - powerDb(r.sumSquares));
    check(off < kLevelNear, name + "at 48 kHz the level is " + std::to_string(off) + " dB off 44.1 kHz's");
}

// A silent input gives the part exactly 0 in the words the host's input goes to (the coordinator's condition,
// 2026-11-25): after a few blocks of it, the part mixer's mic and tracker buffers hold nothing else
void silentInput(TestChecks& check, VSynthEmuProcessor& p, const std::string& name) {
    constexpr int kSilentBlocks = 4;
    juce::AudioBuffer<float> buf(2, kBlock);
    juce::MidiBuffer none;
    for (int b = 0; b < kSilentBlocks; ++b) {
        buf.clear();
        p.processBlock(buf, none);
    }
    const Vc2MixerBuffers& m = p.vc2Play().player()->part().mixer.b;
    const bool zero = std::ranges::all_of(m.mic, [](float x) { return x == 0.0f; })
                      && std::ranges::all_of(m.second, [](float x) { return x == 0.0f; });
    check(zero, name + "a silent input should give the part exactly 0 in the mic's and the tracker's words");
}

void onePatch(TestChecks& check, const DiskPatch& patch, const juce::File& work, double& level, bool other) {
    const juce::File file = work.getChildFile(patch.name);
    writeTestFile(file, patch.bytes);
    Vc2PatchFile read;
    check(vc2PatchRead(patch.bytes.data(), patch.bytes.size(), read), patch.name + " should read");
    auto p = vc2Processor(check, file, kRate);
    const std::string name = patch.name + ": ";
    vc2PlaySystemDefaults(check, *p, name);
    Vc2PlayDirect direct(&read.store);
    const Vc2Played r = play(*p, &direct);
    check(r.differ == 0, name + std::to_string(r.differ) + " samples differ from the part's own, the first at frame " +
                             std::to_string(r.first));
    check(r.finite, name + "a sample out isn't a number");
    check(r.peak > 0, name + "nothing heard");
    check(r.allocations == 0, name + std::to_string(r.allocations) + " allocations in processBlock");
    check(!p->vc2Play().player()->stuck(), name + "the part stopped");
    const Vc2PartPlayer::Counts& c = p->vc2Play().player()->counts();
    check(c.ahead == direct.ahead && c.lost == 0 && c.dropped == 0, name + "the player's waits aren't the part's");
    std::printf("vc2play: %s peak %.4f, %ld blocks (%ld ahead in waits), %ld messages, %ld input frames dropped; "
                "processBlock %.0f%% of real time\n",
                patch.name.c_str(), r.peak, c.blocks, c.ahead, c.messages, c.inputDropped,
                100.0 * r.seconds / ((double)kBlocks * kBlock / kRate));
    // The session: the mode, the store the part played (the moved level in it), in a fresh processor
    juce::MemoryBlock state;
    p->getStateInformation(state);
    VSynthEmuProcessor again;
    again.setStateInformation(state.getData(), (int)state.getSize());
    check(pluginMode(again.apvts.state) == PluginMode::vc2, name + "the session doesn't keep the VC-2 mode");
    check(again.vc2PatchStore() == p->vc2PatchStore() && p->vc2PatchStore() != read.store,
          name + "the session doesn't keep the store the part played");
    if (other) otherRate(check, file, r, name);
    if (other) silentInput(check, *p, name);
    vc2PlaySystemChecks(check, *p, direct, name);
    vc2PlayViewChecks(check, *p, name);   // every patch: a vocoder's may keep a carrier, the others let theirs go
    level = std::max(level, r.peak);
}

int run(const juce::File& rom, const juce::File& work) {
    TestChecks check { "vc2play" };
    vc2PlayImages(check, rom);
    {   // the V-Synth mode makes no part, whatever the control thread's ticks
        VSynthEmuProcessor p;
        p.fxManualTicks();
        p.prepareToPlay(kRate, kBlock);
        p.vc2Play().tick();
        check(!p.vc2Play().vc2() && p.vc2Play().built() == 0, "the V-Synth mode made a VC-2 part");
    }
    // Part 1 receives: rom.4's system defaults set its Receive Switch on, and no parameter of the plugin's holds it
    const Vc2System* system = vc2SystemDefaults();
    check(system != nullptr && vc2SystemValue(*system, Vc2SystemArea::kB, kReceiveSwitch) == 1,
          "rom.4's system defaults should be in, Part 1 Receive Switch on");
    check(std::ranges::none_of(vc2HostParams(), [](const Vc2HostParam& h) { return h.id == kReceiveSwitch; }),
          "no parameter should turn Part 1 Receive Switch off");
    // USB-MIDI Thru off by the same defaults, and no parameter turning it on: the part's MIDI goes to the part
    check(system != nullptr && vc2SystemValue(*system, Vc2SystemArea::kB, kUsbThru) == 0,
          "rom.4's system defaults should have USB-MIDI Thru off");
    check(std::ranges::none_of(vc2HostParams(), [](const Vc2HostParam& h) { return h.id == kUsbThru; }),
          "no parameter should turn USB-MIDI Thru on");
    const std::vector<DiskPatch> patches = diskVc2Patches(rom);
    check(levelParam() != nullptr, "a VC-2 level parameter should be there to move");
    double level = 0;
    for (const size_t k : kPatchesPlayed) {
        if (k < patches.size()) onePatch(check, patches[k], work, level, k == kPatchesPlayed[0]);
        else check(false, "the disk image should hold .p02 " + std::to_string(k));
    }
    if (kFxPatch < patches.size()) vc2PlayFxChecks(check, patches[kFxPatch], work);
    return check.summary("loudest peak " + std::to_string(level));
}

} // namespace

// The check, or with `time` after the image the timing mode (Vc2PlayTime.cpp), with `render` the render mode
// (Vc2PlayRender.cpp)
int main(int argc, char** argv) {
    const auto chosen = [](const juce::File& rom, const juce::File& work, const juce::StringArray& rest) {
        if (rest[0] == "time") return vc2PlayTime(rom, work, rest);
        return rest[0] == "render" ? vc2PlayRender(rom, work, rest) : run(rom, work);
    };
    return testMain(argc, argv, "vc2play", chosen);
}
