// D's rack (UiRack.h; UI step 2; a mode's panels, v453)
#include "UiRack.h"
#include "PluginParamIds.h"
#include "UiLook.h"

namespace {
constexpr int kTripW = 184, kOscW = 310, kCosmH = 110, kAmpDial = 24, kTitleH = 20;

// AMP's knobs, the patch TVA's
AmpPanel::Knobs vsynthAmp() {
    return { { "tvaLevel", "Level" }, { "tvaPan", "Pan" }, { "mfxSend", "MFX" }, { "tvaChoSend", "Chorus" },
             { "tvaRevSend", "Reverb" } };
}

// The V-Synth's panels: its OSC panels and COSM panels by index, AMP and the Time Trip column
Rack::Panels vsynthPanels(VSynthEmuProcessor& p) {
    Rack::Panels made;
    made.trip = std::make_unique<TripColumn>(p);
    for (int osc = 0; osc < Blocks::kOscs; ++osc) made.oscs[(size_t)osc] = std::make_unique<OscPanel>(p, osc);
    for (int cosm = 0; cosm < Blocks::kCosms; ++cosm)
        made.cosms[(size_t)cosm] = std::make_unique<CosmPanel>(p.apvts, cosm);
    made.amp = std::make_unique<AmpPanel>(p.apvts);
    return made;
}
}

AmpPanel::AmpPanel(juce::AudioProcessorValueTreeState& state) : AmpPanel(state, "tvaSwitch", vsynthAmp()) {}

AmpPanel::AmpPanel(juce::AudioProcessorValueTreeState& state, const juce::String& switchId, const Knobs& knobs)
    : FacedPanel(state, { "AMP", switchId, {} }, 1) {
    for (const auto& [id, name] : knobs) addKnob(0, id, name.toRawUTF8(), kAmpDial);
    refresh();
}

void AmpPanel::resized() {
    auto r = getLocalBounds().reduced(Look::inset, 4);
    layoutTitle(r.removeFromLeft(80).withSizeKeepingCentre(80, kTitleH), {});
    const auto& knobs = knobs_.front();
    const int w = knobs.empty() ? 0 : r.getWidth() / (int)knobs.size();
    for (const auto& k : knobs) k->setBounds(r.removeFromLeft(w));
}

Rack::Rack(VSynthEmuProcessor& p) : Rack(vsynthPanels(p)) {}

Rack::Rack(Panels panels) : panels_(std::move(panels)) {
    addAndMakeVisible(*panels_.trip);
    for (int osc = 0; osc < Blocks::kOscs; ++osc) add(*panels_.oscs[(size_t)osc], Blocks::Kind::osc, osc);
    for (int cosm = 0; cosm < Blocks::kCosms; ++cosm) add(*panels_.cosms[(size_t)cosm], Blocks::Kind::cosm, cosm);
    add(*panels_.amp, Blocks::Kind::amp, 0);
    setSelected(Blocks::kFirstSelected);
}

void Rack::add(FacedPanel& panel, Blocks::Kind kind, int index) {
    panel.onSelect = [this, kind, index] {
        if (onSelect) onSelect(Blocks::indexOf(kind, index));
    };
    addAndMakeVisible(panel);
}

// The panel a block has in the rack; MOD has none (its controls are the lower panel's)
FacedPanel* Rack::panelOf(const Blocks::Block& b) {
    const auto i = (size_t)b.index;
    switch (b.kind) {
        case Blocks::Kind::osc: return panels_.oscs[i].get();
        case Blocks::Kind::cosm: return panels_.cosms[i].get();
        case Blocks::Kind::amp: return panels_.amp.get();
        case Blocks::Kind::mod: return nullptr;
    }
    return nullptr;
}

void Rack::setSelected(int block) {
    for (int i = 0; i < Blocks::kCount; ++i)
        if (FacedPanel* panel = panelOf(Blocks::kPath[(size_t)i])) panel->setSelected(i == block);
}

void Rack::refresh() {
    for (const Blocks::Block& b : Blocks::kPath)
        if (FacedPanel* panel = panelOf(b)) panel->refresh();
}

void Rack::resized() {
    auto r = getLocalBounds().withTrimmedBottom(Look::gap).reduced(Look::inset, 0);
    panels_.trip->setBounds(r.removeFromLeft(kTripW));
    for (const auto& osc : panels_.oscs) {
        r.removeFromLeft(Look::gap);
        osc->setBounds(r.removeFromLeft(kOscW));
    }
    r.removeFromLeft(Look::gap);
    for (const auto& cosm : panels_.cosms) {
        cosm->setBounds(r.removeFromTop(kCosmH));
        r.removeFromTop(Look::gap);
    }
    panels_.amp->setBounds(r);
}
