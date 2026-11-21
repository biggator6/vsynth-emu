// xt_fx <rom.4> vc2 [events] [seed]: rom.4's effect driver through XtFxControl (XtFxImage.h), as xt_fx control checks
// rom.2's.  Each part says how many of what it tried were different.
// - Against the reference: through the queue into XtFxChip against the same driver on JE-8086's chip, the whole state each
//   sample, on the programs rom.4's driver loads.  Blind to what the driver is asked: both chips hear the same words.
// - Live edits against a patch: edits one at a time, then the same store loaded as a patch into a new one.
// - Messages and the outside clock (XtFxCheckVc2Hand.cpp): rom.4's driver run by hand against XtFxControl's.
// - The blocks' IDs: what each block's reader reads, run, against the check's own list of what moves that block.
// - The patch EQ: each of its six parameters, live, on the bus, and the same as a patch that has it.
// XT_FX_MAIN only.
#ifdef XT_FX_MAIN
#include "XtFxCheckUnit.h"
#include "XtFxCheckVc2.h"

namespace xtfxcheck {
namespace {
constexpr size_t kFixedRows = 14;                    // kIdBlocks' rows before the types' own
constexpr int kEveryMessage[] = { 0, 1, 2, 0x34 };   // [FW] every message reads Part 1's bank and program, and this
constexpr int kMfxMaster = 0x1b8;                    // [FW] Attenuation's reader reads MFX Master Level as well
constexpr int kChorusReads = 10, kReverbReads = 24;  // [FW] how many IDs from a type's first its reader reads
constexpr int kEqFirst = 0x1af, kEqLast = 0x1b4;     // [FW] EQ 1 and EQ 2: Freq, Q, Gain

// What the readers of a block read: every message's own, then the block's, by the check's list and the current types
std::set<int> expectedReads(XtFxControl& c, Sh4& rom, uint32_t second, uint32_t fourth) {
    std::set<int> s(std::begin(kEveryMessage), std::end(kEveryMessage));
    for (const IdBlock& b : std::span(kIdBlocks).first(kFixedRows)) {
        if (!(b.second & second) && !(b.fourth & fourth)) continue;
        for (int id = b.first; id <= b.last; id++) s.insert(id);
    }
    const XtFxImage& im = vc2Fx();
    const auto typed = [&](uint32_t table, int type, int reads) {
        const int first = (int16_t)rom.rd16(table + 4 * (uint32_t)type);
        const int last = (int16_t)rom.rd16(table + 4 * (uint32_t)type + 2);
        for (int k = 0; k < (reads ? reads : last - first + 1); k++) s.insert(first + k);
    };
    if (fourth & kMfx) typed(im.mfxIds, c.get(vc2Ids().mfxType), 0);
    if (fourth & kChorus) typed(im.choIds, c.get(vc2Ids().choType), kChorusReads);
    if (fourth & kReverb) typed(im.revIds, c.get(vc2Ids().revType), kReverbReads);
    if (fourth & kSwitches) s.insert(kCategory);
    if (second & kAttenuation) s.insert(kMfxMaster);
    return s;
}

void blocksRead(const char* rom) {
    XtFxControl c(rom, nullptr, vc2Fx());
    Sh4 image;
    image.loadRom(rom);
    std::set<int> heard;
    c.observeReads([](void* to, int id) { static_cast<std::set<int>*>(to)->insert(id); }, &heard);
    int tried = 0, different = 0;
    std::string first;
    const auto step = [&](int id, uint32_t second, uint32_t fourth, const char* what) {
        heard.clear();
        editOne(c, id, c.get(id) == c.highest(id) ? c.lowest(id) : c.get(id) + 1);
        tried++;
        if (c.ok() && heard == expectedReads(c, image, second, fourth)) return;
        different++;
        if (first.empty()) first = what;
    };
    const FxIds& fx = vc2Ids();
    step(0x40, kSystem, 0, "the system's");
    step(0x52b, kAttenuation, 0, "Attenuation's");
    step(0x13d, 0, kSwitches, "the switches'");
    for (int t = 0; t < fx.image.mfxTypes; t++) {
        c.set(fx.mfxType, t);
        step(kMfxMaster, 0, kMfx, "an MFX type's");
    }
    for (int t = 0; t < fx.image.choTypes; t++) {
        c.set(fx.choType, t);
        step(0x23d, 0, kChorus, "a Chorus type's");
    }
    for (int t = 0; t < fx.image.revTypes; t++) {
        c.set(fx.revType, t);
        step(0x271, 0, kReverb, "a Reverb type's");
    }
    // The image's counts of types are what the Type parameters' own ranges say
    const std::pair<int, int> counts[] = { { fx.mfxType, fx.image.mfxTypes }, { fx.choType, fx.image.choTypes },
                                           { fx.revType, fx.image.revTypes } };
    for (const auto& [typeId, count] : counts) {
        tried++;
        if (c.highest(typeId) + 1 == count) continue;
        different++;
        if (first.empty()) first = "a count of types";
    }
    // Every ID the check lists counts as a change, whichever block it moves
    for (const IdBlock& b : kIdBlocks) {
        for (int id = b.first; id <= b.last; id++) {
            const int v = c.get(id);
            if (c.lowest(id) == c.highest(id)) continue;
            c.set(id, v == c.highest(id) ? c.lowest(id) : v + 1);
            tried++;
            if (!c.pending()) {
                different++;
                if (first.empty()) first = "ID " + std::to_string(id) + " isn't counted as a change";
            }
            editOne(c, id, v);
        }
    }
    if (different && xtSwitch("XT_FX_FIRST")) std::printf("  the blocks' IDs: first %s\n", first.c_str());
    std::printf("vc2 control's blocks' IDs against their readers: %d samples, %d different\n", tried, different);
}

// Each of the patch EQ's six: a live edit puts words on the bus, the lowest and highest values differ on the chip, and a
// patch that has the value holds the same words
void patchEq(const char* rom) {
    int tried = 0, different = 0;
    for (int id = kEqFirst; id <= kEqLast; id++) {
        Recorder bus;
        XtFxControl live(rom, &bus, vc2Fx());
        editOne(live, id, live.lowest(id));
        const uint64_t before = bus.n;
        const auto low = std::make_unique<XtFxChip>(bus.chip);
        editOne(live, id, live.highest(id));
        const int moved = wordsApart(*low, bus.chip);
        Recorder patchBus;
        XtFxControl patch(rom, &patchBus, vc2Fx());
        editOne(patch, id, live.highest(id), true);
        tried++;
        const int apart = wordsApart(bus.chip, patchBus.chip);
        if (apart && xtSwitch("XT_FX_FIRST")) std::printf("  the patch EQ: ID %x, %d words from a patch\n", id, apart);
        different += bus.n > before && moved > 0 && apart == 0 && live.ok() && patch.ok() ? 0 : 1;
    }
    std::printf("vc2 control's patch EQ on the bus: %d samples, %d different\n", tried, different);
}
} // namespace

const IdBlock* listedBlock(int id) {
    const auto row = std::ranges::find_if(kIdBlocks, [id](const IdBlock& b) { return id >= b.first && id <= b.last; });
    return row == std::end(kIdBlocks) ? nullptr : row;
}

void editOne(XtFxControl& c, int id, int value, bool asPatch) {
    Edit e;
    e.sets.push_back({ id, value });
    e.patch = asPatch;
    apply(c, e);
}

const FxIds& vc2Ids() {
    // [FW] rom.4's IDs: the MFX's, Chorus's and Reverb's levels and sends, Master Level, Output Gain, Attenuation, the
    // Patch Tempo, the three switches, the system's EQ as rom.4's reader reads it, USB External In Type, the patch EQ,
    // and External In Type (v400)
    static constexpr int kLevels[] = { 0x1b8, 0x1b9, 0x1ba, 0x23d, 0x23e, 0x271, 0x40, 0x78, 0x52b, 0x13a, 0x13d, 0x13e,
                                       0x13f, 0x67, 0x68, 0x6f, 0x70, 0x71, 0x72, 0x7f,
                                       0x1af, 0x1b0, 0x1b1, 0x1b2, 0x1b3, 0x1b4, 0x7a };
    // [FW] rom.2's seven of the same types and places (rom2Ids): the firmware's own update takes none of them alone
    static constexpr int kAlone[] = { 0x588, 0x6c8, 0x6ca, 0x5c4, 0x5ea, 0x641, 0x72b };
    static const FxIds ids = { vc2Fx(), "vc2 control", 0x1b7, 0x23c, 0x270, 0x43, 0x13a, kLevels, kAlone };
    return ids;
}

void vc2Control(const char* rom, int events, uint32_t seed) {
    rom4Start(rom);
    againstReference(rom, events, seed, vc2Ids());
    liveEdits(rom, events, seed, vc2Ids());
    handMessages(rom, events, seed);
    handOutsideClock(rom);
    blocksRead(rom);
    patchEq(rom);
    handInput(rom);   // v400: the outside input
    vc2RoutingLaw(rom, seed);
    unitInputVc2(rom, seed);
    unitConversion(rom, seed, vc2Way());   // v274: the frame's way through the unit
    unitLevels(rom, false, vc2Way());
    unitAgainstReference(rom, events, seed, vc2Way());
}
} // namespace xtfxcheck
#endif
