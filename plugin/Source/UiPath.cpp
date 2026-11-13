// D's signal path (UiPath.h; UI step 1)
#include "UiPath.h"
#include "PluginParamIds.h"
#include "UiLook.h"
#include "UiParts.h"

namespace {

using State = juce::AudioProcessorValueTreeState;
using Chip = PathBar::Chip;
constexpr int kChipH = 36, kPadX = 10, kSpace = 6, kArrowW = 16, kStructureW = 92, kNameH = 15;
constexpr int kSlack = 6;   // what a text measured takes more when drawn, so none is cut short
const juce::String kDot = juce::String::fromUTF8(" \xc2\xb7 ");   // " · " between a setting's parts

juce::String textOf(State& s, const juce::String& id) {
    auto* p = s.getParameter(id);
    return p ? p->getCurrentValueAsText() : juce::String();
}

bool isOn(State& s, const juce::String& id) {
    auto* v = s.getRawParameterValue(id);
    return v != nullptr && v->load() > 0.5f;
}

juce::String oscSetting(State& s, int osc) {
    if (!isOn(s, Params::oscId(osc, Params::kOscSwitch))) return "OFF";
    const juce::String type = textOf(s, Params::oscId(osc, Params::kOscType));
    return type == "PCM" ? type + kDot + textOf(s, Params::oscId(osc, Params::kEncodeType)) : type;
}

juce::String modSetting(State& s, int) {
    return isOn(s, Params::kModSwitch) ? textOf(s, Params::kModType) : juce::String("OFF");
}

juce::String cosmSetting(State& s, int cosm) {
    return isOn(s, Params::cosmId(cosm, "Switch")) ? textOf(s, Params::cosmId(cosm, "Type")) : juce::String("OFF");
}

juce::String ampSetting(State& s, int) {
    return "TVA" + kDot + textOf(s, Params::kTvaLevel);
}

// Each kind of block: its name, whether its instances are numbered, and the setting shown under the name
struct KindView {
    const char* name;
    bool numbered;
    juce::String (*setting)(State&, int);
};
constexpr KindView kKinds[] = {   // in Blocks::Kind's order
    { "OSC", true, oscSetting },
    { "MOD", false, modSetting },
    { "COSM", true, cosmSetting },
    { "AMP", false, ampSetting },
};

// The effects' blocks: a name and the switch the chip says ON or OFF of
struct EffectView {
    const char* name;
    const char* id;
};
constexpr EffectView kEffects[] = { { "MFX", "fxMfxOn" }, { "CHORUS", "fxChoOn" }, { "REVERB", "fxRevOn" } };

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

}

PathBar::PathBar(State& state)
    : state_(state) {
    juce::StringArray types;
    for (const juce::String& choice : state.getParameter(Params::kStructure)->getAllValueStrings())
        types.add("TYPE " + choice.getLastCharacters(1));
    structure_.addItemList(types, 1);
    structureAtt_ = std::make_unique<State::ComboBoxAttachment>(state, Params::kStructure, structure_);
    structure_.setTitle("Structure");
    addAndMakeVisible(structure_);
}

void PathBar::setSelected(int block) {
    selected_ = juce::jlimit(0, Blocks::kCount - 1, block);
    repaint();
}

PathBar::Chips PathBar::laidOut() const {
    Chips chips;
    for (const Blocks::Block& b : Blocks::kPath) {
        const KindView& k = kKinds[(size_t)b.kind];
        const juce::String name = k.numbered ? juce::String(k.name) + " " + juce::String(b.index + 1) : k.name;
        chips.blocks.push_back({ name, k.setting(state_, b.index), {}, true });
    }
    for (const EffectView& e : kEffects) {
        const bool on = isOn(state_, e.id);
        chips.effects.push_back({ e.name, on ? "ON" : "OFF", {}, on });
    }
    const int top = (getHeight() - kChipH) / 2;
    int x = Look::inset;
    place(chips.blocks, x, top);
    x += kArrowW;
    place(chips.effects, x, top);
    return chips;
}

void PathBar::refresh() {
    juce::String now;
    const Chips chips = laidOut();
    for (const auto* list : { &chips.blocks, &chips.effects })
        for (const Chip& c : *list) now << c.setting << '|';
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
    for (size_t i = 0; i < chips.blocks.size(); ++i) {
        const bool sel = (int)i == selected_;
        drawChip(g, chips.blocks[i], sel ? ChipColours { Look::accent, Look::ground, Look::ground }
                                         : ChipColours { Look::panel, Look::text, Look::label });
    }
    for (const Chip& c : chips.effects)
        drawChip(g, c, c.on ? ChipColours { Look::panel, Look::text, Look::accent }
                            : ChipColours { Look::panel, Look::muted, Look::muted });
    Ui::drawText(g, "Structure", structure_.getBounds().translated(-70, 0).withWidth(64),
                 { Look::sans(11.0f), Look::label, juce::Justification::centredRight });
}

void PathBar::mouseDown(const juce::MouseEvent& e) {
    const Chips chips = laidOut();
    for (size_t i = 0; i < chips.blocks.size(); ++i)
        if (chips.blocks[i].r.contains(e.getPosition())) {
            setSelected((int)i);
            if (onSelect) onSelect((int)i);
        }
    for (size_t i = 0; i < chips.effects.size(); ++i)
        if (chips.effects[i].r.contains(e.getPosition()) && onEffect) onEffect((int)i);
}
