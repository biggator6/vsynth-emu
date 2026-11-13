// D's rack (UiRack.h; UI step 2)
#include "UiRack.h"
#include "PluginParamIds.h"
#include "UiLook.h"

namespace {
constexpr int kTripW = 184, kOscW = 310, kCosmH = 110, kAmpDial = 24, kTitleH = 20;
// AMP's knobs, the patch TVA's
constexpr Controls::KnobRow kAmpKnobs[] = { { "tvaLevel", "Level" }, { "tvaPan", "Pan" }, { "mfxSend", "MFX" },
                                            { "tvaChoSend", "Chorus" }, { "tvaRevSend", "Reverb" } };
}

AmpPanel::AmpPanel(juce::AudioProcessorValueTreeState& state) : FacedPanel(state, { "AMP", "tvaSwitch", {} }, 1) {
    for (const auto& k : kAmpKnobs) addKnob(0, k.id, k.name, kAmpDial);
    refresh();
}

void AmpPanel::resized() {
    auto r = getLocalBounds().reduced(Look::inset, 4);
    layoutTitle(r.removeFromLeft(80).withSizeKeepingCentre(80, kTitleH), {});
    const auto& knobs = knobs_.front();
    const int w = knobs.empty() ? 0 : r.getWidth() / (int)knobs.size();
    for (const auto& k : knobs) k->setBounds(r.removeFromLeft(w));
}

Rack::Rack(VSynthEmuProcessor& p) : trip_(p), amp_(p.apvts) {
    addAndMakeVisible(trip_);
    for (int osc = 0; osc < Blocks::kOscs; ++osc) {
        oscs_[(size_t)osc] = std::make_unique<OscPanel>(p, osc);
        oscs_[(size_t)osc]->onSelect = [this, osc] { if (onSelect) onSelect(Blocks::indexOf(Blocks::Kind::osc, osc)); };
        addAndMakeVisible(*oscs_[(size_t)osc]);
    }
    for (int cosm = 0; cosm < Blocks::kCosms; ++cosm) {
        cosms_[(size_t)cosm] = std::make_unique<CosmPanel>(p.apvts, cosm);
        cosms_[(size_t)cosm]->onSelect = [this, cosm] {
            if (onSelect) onSelect(Blocks::indexOf(Blocks::Kind::cosm, cosm));
        };
        addAndMakeVisible(*cosms_[(size_t)cosm]);
    }
    amp_.onSelect = [this] { if (onSelect) onSelect(Blocks::indexOf(Blocks::Kind::amp, 0)); };
    addAndMakeVisible(amp_);
    setSelected(Blocks::kFirstSelected);
}

// The panel a block has in the rack; MOD has none (its controls are the lower panel's)
FacedPanel* Rack::panelOf(const Blocks::Block& b) {
    const auto i = (size_t)b.index;
    switch (b.kind) {
        case Blocks::Kind::osc: return oscs_[i].get();
        case Blocks::Kind::cosm: return cosms_[i].get();
        case Blocks::Kind::amp: return &amp_;
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
    trip_.setBounds(r.removeFromLeft(kTripW));
    for (const auto& osc : oscs_) {
        r.removeFromLeft(Look::gap);
        osc->setBounds(r.removeFromLeft(kOscW));
    }
    r.removeFromLeft(Look::gap);
    for (const auto& cosm : cosms_) {
        cosm->setBounds(r.removeFromTop(kCosmH));
        r.removeFromTop(Look::gap);
    }
    amp_.setBounds(r);
}
