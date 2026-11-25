// The VC-2 mode's audio (v455; PluginVc2Play.h, Vc2PartPlayer.h): the processor with the mode switch on the VC-2 plays its
// part, held bit for bit to the part driven directly (Vc2PlayDirect.h), as PluginTest holds the V-Synth to XtSynth.  For a
// few of the disk image's .p02s, each loaded into a fresh processor at 44.1 kHz: a chord and single notes through the
// host's MIDI at frames off the blocks' edges, a controller and a bend, a burst of overlapping notes past the voices, the
// input a voice-like saw on both sides, and a VC-2 parameter moved halfway; every sample out the same as the part's own
// (the processor's output passing OutputFinite, a non-number 0 on both sides), nothing allocated or freed in processBlock.
// Then: the session keeps the mode and the store the part played; the V-Synth mode makes no part; at 48 kHz (the first
// patch) the part is heard through the rate's conversion, finite, near the same level; rom.4's system defaults in, Part 1
// Receive Switch on and USB-MIDI Thru off, and no parameter holding either.  Run: VSynthEmuVc2PlayTest <rom.2.bin>, rom.4
// and the disk image beside it.  Exit 0 if every check holds.
#include "TestChecks.h"
#include "TestMain.h"
#include "TestPatches.h"
#include "Vc2PlayDirect.h"
#include "Vc2PlayRig.h"
#include "../Source/Vc2PatchFile.h"
#include "../Source/Vc2Params.h"
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <new>

namespace {
// What the operators below count: the news and deletes made on a thread while it counts (processBlock's own calls)
struct Allocations {
    static std::atomic<long>& made() {
        static std::atomic<long> n { 0 };
        return n;
    }
    static bool& counting() {
        thread_local bool on = false;
        return on;
    }
    static void* taken(std::size_t n) {
        if (counting()) ++made();
        void* p = std::malloc(n != 0 ? n : 1);
        if (p == nullptr) std::abort();
        return p;
    }
    static void given(void* p) noexcept {
        if (counting() && p != nullptr) ++made();
        std::free(p);
    }
};
} // namespace

void* operator new(std::size_t n) { return Allocations::taken(n); }
void* operator new[](std::size_t n) { return Allocations::taken(n); }
void operator delete(void* p) noexcept { Allocations::given(p); }
void operator delete[](void* p) noexcept { Allocations::given(p); }
void operator delete(void* p, std::size_t) noexcept { Allocations::given(p); }
void operator delete[](void* p, std::size_t) noexcept { Allocations::given(p); }

namespace {
constexpr double kRate = 44100.0, kOtherRate = 48000.0;
constexpr int kBlock = kVc2PlayBlock, kBlocks = 190, kEditAt = 100;   // about 2.2 s; the parameter moved at the 100th
// The disk image's .p02s played, by place, the first at 48 kHz as well.  Left out until v362: 018 FemaleChoirs, which
// plays at nine times real time and nearly silent (peak 1e-4): one note-on's SOLO seek walks records that all read 0
// (vc2SeekRecords through the tones' boundFloat, 1e9 steps), likely v362's fault (Vc2SynthOscOutside::boundFloat
// answering 0 for an address in no tone), and it would take this test past a minute; timed again (this program's
// `time` mode, Vc2PlayTime.cpp) and put back once v362 is in (VC2_MODE_PLAN.md, step 4.5's [gap] on it)
constexpr size_t kPatchesPlayed[] = { 0, 40, 101, 120 };
constexpr int kReceiveSwitch = 0x47, kUsbThru = 0x7d;        // [FW] Part 1 Receive Switch, USB-MIDI Thru: system B's
constexpr float kInputLevel = 0.25f, kPitch = 220.0f;
constexpr double kLevelNear = 6.0;                           // dB the 48 kHz run's level may stand from 44.1 kHz's

using Message = Vc2PlayDirect::Message;

// The host's MIDI: a chord, a controller, a bend, the chord let go, single notes, then a burst past the voices
std::vector<Message> hostMessages() {
    std::vector<Message> m = { { 700, 0x90, 60, 100 },  { 1500, 0x90, 64, 90 },  { 2300, 0x90, 67, 80 },
                               { 30001, 0xb0, 1, 64 },   { 45013, 0xe0, 0, 80 },  { 60000, 0x80, 60, 0 },
                               { 60007, 0x80, 64, 0 },   { 60011, 0x80, 67, 0 },  { 61000, 0x91, 62, 110 },
                               { 70100, 0x81, 62, 64 } };
    for (int k = 0; k < 40; ++k) m.push_back({ 72000 + 97 * k, 0x90, (uint8_t)(48 + k % 24), (uint8_t)(40 + k) });
    for (int k = 0; k < 40; ++k) m.push_back({ 80000 + 53 * k, 0x80, (uint8_t)(48 + k % 24), 0 });
    return m;
}

// The input: a saw at kPitch, a little higher on the right, at kInputLevel
float inputAt(int frame, int side) {
    const double cycle = frame * (kPitch * (side ? 1.01 : 1.0)) / kRate;
    return kInputLevel * (float)(2.0 * (cycle - std::floor(cycle)) - 1.0);
}

// A VC-2 parameter of zone 1 named as a level, its value moved: the first whose name says Level
const Vc2HostParam* levelParam() {
    for (const Vc2HostParam& h : vc2HostParams()) {
        const bool stored = h.zone == 0 && h.cosm == 0 && h.id >= kVc2FirstStored;
        if (stored && vc2ParamName(h.id).find("Level") != std::string::npos) return &h;
    }
    return nullptr;
}

struct Played {
    long differ = 0, first = -1, allocations = 0;
    double peak = 0, sumSquares = 0, seconds = 0;   // seconds: processBlock's, all of them
    bool finite = true;
};

// The processor's blocks against the direct part's at 44.1 kHz (none at another rate)
Played play(VSynthEmuProcessor& p, Vc2PlayDirect* direct) {
    Played r;
    const std::vector<Message> messages = hostMessages();
    const Vc2HostParam* h = levelParam();
    juce::AudioBuffer<float> buf(2, kBlock);
    std::vector<float> wantL(kBlock), wantR(kBlock), inL(kBlock), inR(kBlock);
    for (int b = 0; b < kBlocks; ++b) {
        const int from = b * kBlock;
        if (b == kEditAt && h != nullptr) {   // a level moved: the processor's parameter, the direct part's store
            auto* prm = p.apvts.getParameter(h->paramId);
            const int now = (int)std::lround(prm->convertFrom0to1(prm->getValue()));
            const int lo = (int)prm->getNormalisableRange().start, hi = (int)prm->getNormalisableRange().end;
            const int v = (now + lo) / 2 != now ? (now + lo) / 2 : hi;   // moved, halfway down or to the top
            prm->setValueNotifyingHost(prm->convertTo0to1((float)v));
            p.vc2Play().tick();
            if (direct != nullptr) direct->edit(*h, v);
        }
        juce::MidiBuffer midi;
        for (const Message& m : messages)
            if (m.frame >= from && m.frame < from + kBlock)
                midi.addEvent(juce::MidiMessage(m.status, m.data1, m.data2), m.frame - from);
        for (int i = 0; i < kBlock; ++i) {
            inL[(size_t)i] = inputAt(from + i, 0);
            inR[(size_t)i] = inputAt(from + i, 1);
        }
        buf.copyFrom(0, 0, inL.data(), kBlock);
        buf.copyFrom(1, 0, inR.data(), kBlock);
        const auto began = std::chrono::steady_clock::now();
        Allocations::counting() = true;
        p.processBlock(buf, midi);
        Allocations::counting() = false;
        r.seconds += std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count();
        if (direct != nullptr) {   // denormals flushed, as the SH-4 and processBlock have them
            const juce::ScopedNoDenormals flushed;
            direct->host(inL.data(), inR.data(), kBlock, messages, wantL.data(), wantR.data());
        }
        for (int i = 0; i < kBlock; ++i)
            for (int ch = 0; ch < 2; ++ch) {
                const float got = buf.getSample(ch, i), want = (ch ? wantR : wantL)[(size_t)i];
                r.finite = r.finite && std::isfinite(got);
                r.peak = std::max(r.peak, (double)std::fabs(got));
                r.sumSquares += (double)got * got;
                if (direct == nullptr || got == (std::isfinite(want) ? want : 0.0f)) continue;
                if (r.first < 0) r.first = from + i;
                ++r.differ;
            }
    }
    r.allocations = Allocations::made().exchange(0);
    return r;
}

// A run's mean power in dB, from its sum of squares over every frame played
double powerDb(double sumSquares) {
    return 10.0 * std::log10(std::max(sumSquares / ((double)kBlocks * kBlock), 1e-30));
}

// At 48 kHz: the part through the rate's conversion, finite, near the level the 44.1 kHz run `r` had
void otherRate(TestChecks& check, const juce::File& file, const Played& r, const std::string& name) {
    auto other = vc2Processor(check, file, kOtherRate);
    const Played o = play(*other, nullptr);
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
    Vc2PlayDirect direct(&read.store);
    const Played r = play(*p, &direct);
    const std::string name = patch.name + ": ";
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
    return check.summary("loudest peak " + std::to_string(level));
}

} // namespace

// The check, or with `time` after the image the timing mode (Vc2PlayTime.cpp)
int main(int argc, char** argv) {
    const auto chosen = [](const juce::File& rom, const juce::File& work, const juce::StringArray& rest) {
        return rest[0] == "time" ? vc2PlayTime(rom, work, rest) : run(rom, work);
    };
    return testMain(argc, argv, "vc2play", chosen);
}
