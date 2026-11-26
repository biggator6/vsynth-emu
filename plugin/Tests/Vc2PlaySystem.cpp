// VSynthEmuVc2PlayTest's checks of the system's parameters (v457; PluginVc2Play.h, Vc2PartPlayer::systemGiven), each read
// as the part's readers read it (Vc2SynthSettings::value on its reader's block), not through the plugin's own map of the
// areas (vc2SystemAreaOf).  On a part just made: each the part reads (Clock Source, External In Type, the controller
// assigns) and v458 the effects' two (Master Level, Output Gain) at its default, that default rom.4's system's as a part
// given rom.4's defaults alone reads it; all 18 kept.  After a run that moved some (Vc2PlayTest.cpp's play, bit for bit
// against the part driven directly): the part reading each parameter's value, its settings' tables and the Matrix's
// settings what the direct part's readers wrote (the front's system and assignments messages run: rom.4's stop after
// them stops no voice here, so the sound alone doesn't show them), and a part made from the session reading the same.
#include "Vc2PlayDirect.h"
#include "Vc2PlayRig.h"
#include "../Source/UiControls.h"
#include "../Source/Vc2Params.h"
#include "../Source/Vc2SynthSettings.h"
#include "../Source/Vc2SystemStore.h"
#include <cstring>
#include <tuple>

namespace {

// [FW] a system ID the part reads and the block its reader reads it on
struct Read {
    int id;
    Vc2ParamBlock block;
};
// Clock Source the clock's (0x89324350 on 0x89320760), External In Type the system dispatcher's trailer (0x8939b0d0),
// system B's; Beam 2 L to Knob 2 the assignments' dispatcher's (0x8939b110) and Time Trip's, the system's, with Beam 1's
// (0x89, 0x8a) beside them, which no reader of the part reads; v458 Master Level and Output Gain, the effects' system
// reader's (0x8935feb0, traced on 0x89320760), system B's, which no reader of the part reads either
std::vector<Read> reads() {
    std::vector<Read> r = { { 0x43, Vc2ParamBlock::kSystemB }, { 0x7a, Vc2ParamBlock::kSystemB },
                            { 0x40, Vc2ParamBlock::kSystemB }, { 0x78, Vc2ParamBlock::kSystemB } };
    for (int id = 0x89; id <= 0x96; ++id) r.push_back({ id, Vc2ParamBlock::kSystem });
    return r;
}

// What a part's readers read of it
int readOnItsBlock(Vc2Synth& part, const Read& r) { return part.settings->value(r.block, 0, 0, (int16_t)r.id); }

// The first the processor's part doesn't read at its parameter's value (with `rom4`, a part given rom.4's defaults
// alone: its read of it the parameter's default and value as well), 0 for none, -1 with no part
int unheld(VSynthEmuProcessor& p, Vc2Synth* rom4) {
    Vc2PartPlayer* player = p.vc2Play().player();
    if (player == nullptr) return -1;
    for (const Read& r : reads()) {
        const juce::String id = vc2HostParamId(r.id, 0, 0);
        auto* prm = p.apvts.getParameter(id);
        const int value = Controls::paramValue(p.apvts, id);
        const int def = (int)std::lround(prm->convertFrom0to1(prm->getDefaultValue()));
        const bool asRom4 = rom4 == nullptr || (readOnItsBlock(*rom4, r) == value && def == value);
        if (!asRom4 || readOnItsBlock(player->part(), r) != value) return r.id;
    }
    return 0;
}

std::string named(int id) { return id > 0 ? vc2ParamName(id) : std::string(id == 0 ? "none" : "no part"); }

// The tables the system's and assignments' dispatchers write, and the rest of the part's settings with them
auto tables(const Vc2PartRows& t) {
    return std::tie(t.commons, t.envelopes, t.lfos, t.mixes, t.tvas, t.controls, t.bytes, t.cosms, t.oscs, t.steps,
                    t.robot, t.modes, t.heldNote, t.tripAssigns, t.patchRemain, t.voiceCounts, t.chordMemory,
                    t.holdPresses);
}
} // namespace

std::unique_ptr<Vc2Synth> vc2PlayRom4System() {
    auto rom4 = std::make_unique<Vc2Synth>();
    if (const Vc2System* system = vc2SystemDefaults()) rom4->giveSystem(*system);
    return rom4;
}

void vc2PlaySystemDefaults(TestChecks& check, VSynthEmuProcessor& p, const std::string& name) {
    const auto rom4 = vc2PlayRom4System();
    const int first = unheld(p, rom4.get());
    check(first == 0, name + named(first) + " should start at its default, rom.4's system's as the part reads it");
    int mapped = 0;
    Vc2SystemArea area {};
    for (const Vc2HostParam& h : vc2HostParams()) mapped += vc2SystemAreaOf(h.id, area) ? 1 : 0;
    check(mapped == (int)reads().size(),
          name + "the plugin should keep all 18 system parameters on their areas, not " + std::to_string(mapped));
}

void vc2PlaySystemChecks(TestChecks& check, VSynthEmuProcessor& p, const Vc2PlayDirect& direct,
                         const std::string& name) {
    int first = unheld(p, nullptr);
    check(first == 0, name + "the part doesn't read " + named(first) + " at its parameter's value");
    const Vc2Synth& mine = p.vc2Play().player()->part();
    const Vc2Synth& theirs = direct.part();
    check(tables(mine.rows) == tables(theirs.rows) && std::memcmp(&mine.g, &theirs.g, sizeof mine.g) == 0,
          name + "the part's settings' tables or the Matrix's settings aren't what the direct part's readers wrote");
    juce::MemoryBlock state;
    p.getStateInformation(state);
    VSynthEmuProcessor again;
    again.fxManualTicks();
    again.prepareToPlay(44100.0, kVc2PlayBlock);
    again.setStateInformation(state.getData(), (int)state.getSize());
    again.vc2Play().tick();
    first = unheld(again, nullptr);
    check(first == 0, name + "a part made from the session doesn't read " + named(first) + " at its parameter's value");
}
