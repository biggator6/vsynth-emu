// D's signal path (UiPath.h; UI step 1; a mode's, v453)
#include "UiPath.h"
#include "PluginParamIds.h"
#include "UiBlockKinds.h"
#include "UiControls.h"
#include "UiFooter.h"
#include "UiLook.h"
#include "UiParts.h"

namespace {

using State = juce::AudioProcessorValueTreeState;
using Chip = PathBar::Chip;
constexpr int kChipH = 36, kPadX = 10, kSpace = 6, kArrowW = 16, kStructureW = 92, kNameH = 15;
constexpr int kSlack = 6;   // what a text measured takes more when drawn, so none is cut short
constexpr int kZones = 2;   // the VC-2's zones on the path (Current Zone, ZONE1 and ZONE2)
const juce::String kDot = juce::String::fromUTF8(" \xc2\xb7 ");   // " · " between a setting's parts

bool isOn(State& s, const juce::String& id) {
    auto* v = s.getRawParameterValue(id);
    return v != nullptr && v->load() > 0.5f;
}

juce::String oscSetting(State& s, int osc) {
    if (!isOn(s, Params::oscId(osc, Params::kOscSwitch))) return "OFF";
    const juce::String type = Controls::currentText(s, Params::oscId(osc, Params::kOscType));
    return type == "PCM" ? type + kDot + Controls::currentText(s, Params::oscId(osc, Params::kEncodeType)) : type;
}

juce::String modSetting(State& s, int) {
    return isOn(s, Params::kModSwitch) ? Controls::currentText(s, Params::kModType) : juce::String("OFF");
}

juce::String cosmSetting(State& s, int cosm) {
    return isOn(s, Params::cosmId(cosm, "Switch")) ? Controls::currentText(s, Params::cosmId(cosm, "Type"))
                                                   : juce::String("OFF");
}

juce::String ampSetting(State& s, int) {
    return "TVA" + kDot + Controls::currentText(s, Params::kTvaLevel);
}

// The setting each kind of block shows under its name, in Blocks::Kind's order
using Setting = juce::String (*)(State&, int);
constexpr Setting kSettings[] = { oscSetting, modSetting, cosmSetting, ampSetting };

juce::String vsynthSetting(State& s, const Blocks::Block& b) {
    return kSettings[(size_t)b.kind](s, b.index);
}

// Each chip as wide as its longer line, from x on, with the space between
void place(std::vector<Chip>& chips, int& x, int top) {
    const juce::Font name = Look::sans(11.0f, "Bold"), setting = Look::sans(10.0f);
    for (Chip& c : chips) {
        const float w = juce::jmax(juce::GlyphArrangement::getStringWidth(name, c.name),
                                   juce::GlyphArrangement::getStringWidth(setting, c.setting));
        c.r = { x, top, (int)std::ceil(w) + 2 * kPadX + kSlack, kChipH };
        x = c.r.getRight() + kSpace;
    }
}

// How a chip is drawn: its fill, its name's colour and its setting's
struct ChipColours {
    juce::Colour fill, name, setting;
};

void drawChip(juce::Graphics& g, const Chip& c, const ChipColours& colours) {
    Ui::fillRounded(g, c.r.toFloat(), colours.fill, 6.0f);
    const auto r = c.r.reduced(kPadX, 4);
    Ui::drawText(g, c.name, r.withHeight(kNameH), { Look::sans(11.0f, "Bold"), colours.name });
    Ui::drawText(g, c.setting, r.withTrimmedTop(kNameH), { Look::sans(10.0f), colours.setting });
}

// An effect's or the VOCODER's chip: lit when on, muted when off
ChipColours switchColours(const Chip& c) {
    if (c.on) return { Look::panel, Look::text, Look::accent };
    return { Look::panel, Look::muted, Look::muted };
}

}

PathBar::Mode PathBar::vsynth() {
    const std::array<Effect, 3> effects { { { "MFX", "fxMfxOn" }, { "CHORUS", "fxChoOn" }, { "REVERB", "fxRevOn" } } };
    return { Blocks::kPath, vsynthSetting, effects, Params::kStructure, {}, nullptr };
}

PathBar::PathBar(State& state, const Mode& mode)
    : state_(state), mode_(mode) {
    juce::StringArray types;
    const auto range = state.getParameter(mode.structureId)->getNormalisableRange();
    for (int v = juce::roundToInt(range.start); v <= juce::roundToInt(range.end); ++v)
        types.add("TYPE " + Controls::editorValueText(state, mode.structureId, v).getLastCharacters(1));
    structure_.addItemList(types, 1);
    structureAtt_ = std::make_unique<State::ComboBoxAttachment>(state, mode.structureId, structure_);
    structure_.setTitle("Structure");
    addAndMakeVisible(structure_);
}

void PathBar::setSelected(int block) {
    selected_ = juce::jlimit(0, Blocks::kCount - 1, block);
    repaint();
}

int PathBar::zone() const {
    const auto* v = mode_.zoneId.isEmpty() ? nullptr : state_.getRawParameterValue(mode_.zoneId);
    return v != nullptr ? juce::roundToInt(v->load()) : 0;
}

PathBar::Chips PathBar::laidOut() const {
    Chips chips;
    for (int z = 0; z < (mode_.zoneId.isEmpty() ? 0 : kZones); ++z)
        chips.zones.push_back({ "Z" + juce::String(z + 1), {}, {}, z == zone() });
    for (const Blocks::Block& b : mode_.path) {
        chips.blocks.push_back({ Blocks::blockName(b), mode_.setting(state_, b), {}, true });
    }
    if (mode_.vocoder != nullptr) {
        const juce::String line = mode_.vocoder(state_);
        chips.vocoder.push_back({ "VOCODER", line, {}, line == "LEGACY" || line == "HIRESO" });
    }
    for (const Effect& e : mode_.effects) {
        const bool on = isOn(state_, e.switchId);
        chips.effects.push_back({ e.name, on ? "ON" : "OFF", {}, on });
    }
    const int top = (getHeight() - kChipH) / 2;
    int x = Look::inset;
    for (auto* list : { &chips.zones, &chips.blocks, &chips.vocoder }) place(*list, x, top);
    x += kArrowW;
    place(chips.effects, x, top);
    return chips;
}

void PathBar::refresh() {
    juce::String now;
    const Chips chips = laidOut();
    for (const auto* list : { &chips.zones, &chips.blocks, &chips.vocoder, &chips.effects })
        for (const Chip& c : *list) now << c.setting << (int)c.on << '|';
    if (now == shown_) return;
    shown_ = now;
    repaint();
}

void PathBar::resized() {
    structure_.setBounds(getLocalBounds().removeFromRight(kStructureW + Look::inset).withTrimmedRight(Look::inset)
                             .withSizeKeepingCentre(kStructureW, 24));
}

void PathBar::paint(juce::Graphics& g) {
    const Chips chips = laidOut();
    for (size_t z = 0; z < chips.zones.size(); ++z) {
        const Chip& c = chips.zones[z];
        const juce::Colour colour = Footer::zoneColour((int)z);
        drawChip(g, c, c.on ? ChipColours { colour, Look::ground, Look::ground }
                            : ChipColours { Look::panel, colour, Look::label });
    }
    for (size_t i = 0; i < chips.blocks.size(); ++i) {
        const bool sel = (int)i == selected_;
        drawChip(g, chips.blocks[i], sel ? ChipColours { Look::accent, Look::ground, Look::ground }
                                         : ChipColours { Look::panel, Look::text, Look::label });
    }
    for (const auto* list : { &chips.vocoder, &chips.effects })
        for (const Chip& c : *list) drawChip(g, c, switchColours(c));
    Ui::drawText(g, "Structure", structure_.getBounds().translated(-70, 0).withWidth(64),
                 { Look::sans(11.0f), Look::label, juce::Justification::centredRight });
}

void PathBar::mouseDown(const juce::MouseEvent& e) {
    const Chips chips = laidOut();
    for (size_t z = 0; z < chips.zones.size(); ++z)
        if (chips.zones[z].r.contains(e.getPosition())) Controls::setValue(state_, mode_.zoneId, (int)z);
    for (size_t i = 0; i < chips.blocks.size(); ++i)
        if (chips.blocks[i].r.contains(e.getPosition())) {
            setSelected((int)i);
            if (onSelect) onSelect((int)i);
        }
    for (const Chip& c : chips.vocoder)
        if (c.r.contains(e.getPosition()) && onVocoder) onVocoder();
    for (size_t i = 0; i < chips.effects.size(); ++i)
        if (chips.effects[i].r.contains(e.getPosition()) && onEffect) onEffect((int)i);
}
