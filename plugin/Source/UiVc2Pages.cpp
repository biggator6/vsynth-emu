// The VC-2 mode's pages (UiVc2Pages.h; v453)
#include "UiVc2Pages.h"
#include "PluginVc2Params.h"
#include "PluginVc2Patch.h"
#include "UiBars.h"
#include "UiBlocks.h"
#include "UiModPage.h"
#include "UiPages.h"
#include "UiPlayPage.h"
#include "UiVc2FxPage.h"
#include "UiVc2Kinds.h"
#include "UiVc2VocoderPage.h"
#include "UiZonesPage.h"
#include <array>

namespace Vc2Pages {

namespace {

// [FW] The MODULATION page's assigns (UI_DESIGN_VC2.md, "MODULATION"): the system's controller assigns, Beam Type after
const Controls::KnobRow kAssigns[] = {
    { "89", "Beam 1 L" }, { "8a", "Beam 1 R" }, { "8b", "Beam 2 L" }, { "8c", "Beam 2 R" }, { "8d", "Beam 3 L" },
    { "8e", "Beam 3 R" }, { "8f", "Beam 4 L" }, { "90", "Beam 4 R" }, { "91", "Pad X" },    { "92", "Pad Y" },
    { "93", "Trip X" },   { "94", "Trip Y" },   { "95", "Knob 1" },   { "96", "Knob 2" },   { "4a6", "Beam Type" },
};

// The PLAY page's cards (UI_DESIGN_VC2.md, "PLAY"): the arpeggio down the left; the patch and tempo, portamento (its
// switch on its title), keys and tune down the right, Scale Tune last
const Pages::CardRows kPlayCards[] = {
    { "ARPEGGIO",
      { { { "4a9", "Arpeggio" }, { "4aa", "Hold" }, { "4ab", "Motif" }, { "4b0", "Octaves" }, { "4af", "Velocity" },
          { "4ac", "Duration" }, { "4b1", "Grid" }, { "4b2", "End Point" }, { "4ad", "Shuffle" },
          { "4ae", "Resolution" } } },
      0 },
    { "PATCH & TEMPO", { { { "12c", "Category" }, { "43", "Clock" }, { "13a", "Tempo" } } }, 1 },
    { "PORTAMENTO",
      { { { "133", "Mode" }, { "134", "Type" }, { "135", "Time" }, { "136", "Velo Sens" } } }, 1, "132" },
    { "KEYS & TUNE",
      { { { "130", "Voices" }, { "131", "Legato" }, { "12f", "Octave" }, { "12d", "Coarse" }, { "12e", "Fine" },
          { "13b", "Bend Up" }, { "13c", "Bend Down" } } },
      1 },
};
constexpr int kScaleSwitch = 0x151, kScaleFirst = 0x153, kScaleStep = 2;   // [FW] C, then every other ID to B
constexpr int kKeys = 12, kFirstUpper = 0x141, kPartZones = 2;

// SCALE TUNE (d-7): its switch on its title, twelve upright bars, a key each
std::unique_ptr<Card> scaleTune(Controls::State& state) {
    static const char* const kNames[kKeys] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    ControlGrid::Rows rows;
    for (int k = 0; k < kKeys; ++k) rows.push_back({ vc2HostParamId(kScaleFirst + kScaleStep * k, 0, 0), kNames[k] });
    auto card = std::make_unique<Card>("SCALE TUNE");
    auto on = std::make_unique<Pages::TitleSwitch>(state, vc2HostParamId(kScaleSwitch, 0, 0));
    card->toTitle(std::move(on), Pages::TitleSwitch::kWidth);
    card->setBody(std::make_unique<UprightBars>(state, rows));
    return card;
}

std::unique_ptr<juce::Component> modulation(VSynthEmuProcessor& p) {
    ModulationPage::Mode mode { false, Vc2Blocks::matrixId, {} };
    for (const Controls::KnobRow& row : kAssigns) mode.assigns.push_back({ patchId(row.id), row.name });
    return std::make_unique<ModulationPage>(p, mode);
}

std::unique_ptr<juce::Component> play(VSynthEmuProcessor& p) {
    PlayPage::Mode mode { { std::begin(kPlayCards), std::end(kPlayCards) }, patchId,
                          { [&p] { return vc2ArpPattern(p.vc2PatchStore()); }, "vc2Store", patchId("4b2") },
                          [&p] { return scaleTune(p.apvts); } };
    return std::make_unique<PlayPage>(p, std::move(mode));
}

// What a zone plays: zones 1 and 2 the SYNTH page's; the others the .p02's data, which the session keeps, or none
juce::String zonePlays(VSynthEmuProcessor& p, int zone) {
    if (zone < kPartZones) return "the SYNTH page's zone " + juce::String(zone + 1);
    return p.vc2PatchStore().empty() ? juce::String("no patch, silent") : juce::String("the .p02's data");
}

juce::String zoneUpper(int zone) {
    return vc2HostParamId(kFirstUpper + zone, 0, 0);
}

std::unique_ptr<juce::Component> zones(VSynthEmuProcessor& p) {
    return std::make_unique<ZonesPage>(p, ZonesPage::Mode { zoneUpper, "vc2Store", zonePlays });
}

// Each page's maker, in Blocks::kVc2Pages' order: SYNTH, VOCODER, MODULATION, FX, PLAY, ZONES
constexpr std::array<Pages::Maker, Blocks::kVc2Pages.size()> kMakers {
    Pages::noPage, Pages::made<Vc2VocoderPage>, modulation, Pages::made<Vc2FxPage>, play, zones
};

}

juce::String patchId(const char* row) {
    return vc2HostParamId(juce::String(row).getHexValue32(), 0, 0);
}

std::unique_ptr<juce::Component> makeVc2Page(int page, VSynthEmuProcessor& p) {
    return kMakers[(size_t)page](p);
}

}
