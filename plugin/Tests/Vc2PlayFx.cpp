// VSynthEmuVc2PlayTest's effects run (v458; PluginVc2Fx.h): one .p02 with the Effects switch on, the processor against
// the part driven directly through a Vc2Fx of its own on rom.4, driven by hand, bit for bit, as verify.sh's "effects"
// line holds the V-Synth's (TEST_FX_KNOBS, TEST_FX_LIVE): the control thread ticked before every block on both sides,
// the effects edited in play (MFX Type, the MFX switch off and on again, Reverb Type, Chorus Type), the Effects switch
// off for ten blocks (the copy without the chip, the driver ticked on), CC 91 and 93 among the host's MIDI.  The effects
// heard (the run unlike the same patch's without them), the driver's system IDs rom.4's system as its getter reads them
// on the blocks the driver was traced reading them on (not through vc2FxSystemAreaOf), nothing allocated in
// processBlock, and the time processBlock took per second of audio, with the effects and without, the first under real
// time.
#include "TestPatches.h"
#include "Vc2PlayDirect.h"
#include "Vc2PlayRig.h"
#include "../Source/PluginParamIds.h"
#include "../Source/Vc2Params.h"
#include "../Source/Vc2PatchFile.h"
#include "../Source/Vc2PatchStore.h"
#include "../Source/Vc2SynthSettings.h"
#include "../Source/XtFiles.h"
#include <cstdio>

namespace {
constexpr double kRate = 44100.0;
constexpr int kOffAt = 150, kOnAt = 160;   // the Effects switch off, then on again
// [FW] rom.4's IDs edited in play, each at its block: MFX Type, the MFX switch (off, then on), Reverb Type, Chorus Type
struct FxEdit {
    int block, id;
};
constexpr FxEdit kEdits[] = { { 40, 0x1b7 }, { 70, 0x13d }, { 74, 0x13d }, { 110, 0x270 }, { 130, 0x23c } };
// [FW] the driver's system IDs and the read object it reads each on, as traced (HARDWARE_FIRMWARE.md, "The VC-2 mode's
// effects"): its system reader's (0x8935feb0) on system B's 0x89320760, Attenuation's on system C's 0x89320f60
struct FxRead {
    int id;
    Vc2ParamBlock block;
};
constexpr FxRead kTraced[] = { { 0x40, Vc2ParamBlock::kSystemB },  { 0x43, Vc2ParamBlock::kSystemB },
                               { 0x67, Vc2ParamBlock::kSystemB },  { 0x68, Vc2ParamBlock::kSystemB },
                               { 0x6f, Vc2ParamBlock::kSystemB },  { 0x70, Vc2ParamBlock::kSystemB },
                               { 0x71, Vc2ParamBlock::kSystemB },  { 0x72, Vc2ParamBlock::kSystemB },
                               { 0x78, Vc2ParamBlock::kSystemB },  { 0x7a, Vc2ParamBlock::kSystemB },
                               { 0x7f, Vc2ParamBlock::kSystemB },  { 0x52b, Vc2ParamBlock::kSystemC } };

const Vc2HostParam* zoneOneParam(int id) {
    for (const Vc2HostParam& h : vc2HostParams())
        if (h.id == id && h.zone == 0 && h.cosm == 0) return &h;
    return nullptr;
}

// The block's edits on `p` (and with `direct`, on its store as well)
void editsAt(int b, VSynthEmuProcessor& p, Vc2PlayDirect* direct) {
    for (const FxEdit& e : kEdits) {
        const Vc2HostParam* h = zoneOneParam(e.id);
        if (e.block != b || h == nullptr) continue;
        const int v = vc2PlayMove(p, h->paramId);
        if (direct != nullptr) direct->edit(*h, v);
    }
}

// The processor's run with the effects, against `direct`'s: the edits, the switch, both sides' ticks before each block
Vc2Played withEffects(VSynthEmuProcessor& p, Vc2PlayDirect& direct) {
    return vc2PlayRun(p, &direct, [&](int b) {
        editsAt(b, p, &direct);
        if (b == kOffAt || b == kOnAt) {
            p.apvts.getParameter(Params::kFxOn)->setValueNotifyingHost(b == kOnAt ? 1.0f : 0.0f);
            direct.fxHeard(b == kOnAt);
        }
        p.vc2Play().tick();
        direct.fxTick();
    });
}

// [FW] the patch's IDs the driver's readers read (VC2_MODE_PLAN.md, step 4.4's table): the switches with the patch EQ and
// the Category, the MFX's, Chorus's and Reverb's own, and on rom.4 alone 0x195, 0x2e0, 0x2e2 and 0x2e5; the effect
// types' own IDs, typed, from the second bank's first to the COSMs'
constexpr int kPatchRead[] = { 0x13a, 0x13d, 0x13e, 0x13f, 0x1af, 0x1b0, 0x1b1, 0x1b2, 0x1b3, 0x1b4, 0x12c, 0x1b7,
                               0x1b8, 0x1b9, 0x1ba, 0x23c, 0x23d, 0x23e, 0x270, 0x271, 0x195, 0x2e0, 0x2e2, 0x2e5 };
constexpr int kTypedFirst = 0x56a, kTypedEnd = 0x926;

// The first of the driver's IDs whose value isn't rom.4's as its getter reads it, or 0: the system's on the blocks it
// was traced reading them on, the patch's from the store the part played
int unlikeRom4(VSynthEmuProcessor& p) {
    const auto rom4 = vc2PlayRom4System();
    const PluginVc2Fx& fx = p.vc2Play().fx();
    for (const FxRead& r : kTraced)
        if (fx.driverValue(r.id) != rom4->settings->value(r.block, 0, 0, (int16_t)r.id)) return r.id;
    const std::vector<uint8_t> store = p.vc2PatchStore();
    for (const int id : kPatchRead)
        if (fx.driverValue(id) != xtStoreValue(vc2Store(), store, id)) return id;
    for (int id = kTypedFirst; id < kTypedEnd; ++id)
        if (fx.driverValue(id) != vc2StoreTyped(store, id)) return id;
    return 0;
}
} // namespace

void vc2PlayFxChecks(TestChecks& check, const DiskPatch& patch, const juce::File& work) {
    const juce::File file = work.getChildFile(patch.name);
    writeTestFile(file, patch.bytes);
    Vc2PatchFile read;
    check(vc2PatchRead(patch.bytes.data(), patch.bytes.size(), read), patch.name + " should read");
    const std::string name = patch.name + " with the effects: ";
    for (const FxEdit& e : kEdits)
        check(zoneOneParam(e.id) != nullptr, name + "no parameter for " + vc2ParamName(e.id) + " to edit");
    auto dry = vc2Processor(check, file, kRate);
    const Vc2Played without = vc2PlayRun(*dry, nullptr, [&](int b) {
        editsAt(b, *dry, nullptr);
        dry->vc2Play().tick();
    });
    auto p = vc2Processor(check, file, kRate);
    p->apvts.getParameter(Params::kFxOn)->setValueNotifyingHost(1.0f);
    p->vc2Play().tick();   // the unit made and started, on the control thread
    check(p->vc2Play().fx().sink() != nullptr, name + "the effects didn't start: " + p->vc2Play().fx().failed());
    const std::string rom4 = xtSynthVc2RomPath();
    Vc2PlayDirect direct(&read.store, &rom4);
    check(direct.fxOk(), name + "the direct part's effects didn't start");
    const Vc2Played r = withEffects(*p, direct);
    check(r.differ == 0, name + std::to_string(r.differ) + " samples differ from the part's own through its effects, " +
                             "the first at frame " + std::to_string(r.first));
    check(r.finite && r.peak > 0, name + "nothing heard, or a sample out isn't a number");
    check(r.sumSquares != without.sumSquares, name + "the run is the same as without the effects");
    const int unlike = unlikeRom4(*p);
    check(unlike == 0, name + "the driver's " + vc2ParamName(unlike) + " (0x" + juce::String::toHexString(unlike)
                                  .toStdString() + ") isn't rom.4's as its getter reads it");
    check(r.allocations == 0, name + std::to_string(r.allocations) + " allocations in processBlock");
    const double audio = (double)kVc2PlayBlocks * kVc2PlayBlock / kRate;
    check(r.seconds < audio, name + "processBlock took longer than real time");
    std::printf("vc2play effects: %s peak %.4f (%.4f without); processBlock %.0f%% of real time (%.0f%% without)\n",
                patch.name.c_str(), r.peak, without.peak, 100.0 * r.seconds / audio, 100.0 * without.seconds / audio);
}
