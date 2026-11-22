// The VC-2 mode's SYNTH page (UiVc2SynthPage.h; v453)
#include "UiVc2SynthPage.h"
#include "PluginVc2Params.h"
#include "UiBlockKinds.h"
#include "UiLook.h"
#include "UiVc2Kinds.h"
#include "UiVc2Mic.h"
#include "UiVc2Rack.h"

namespace {

using State = juce::AudioProcessorValueTreeState;
constexpr int kCurrentZone = 0x140;   // [FW] ZONE1, ZONE2
const juce::String kDot = juce::String::fromUTF8(" \xc2\xb7 ");

// Each kind's switch and the row its chip says when it is on, OSC 1's and COSM 1's rows (AMP's its level, always on)
struct ChipRows {
    const char* on;
    const char* says;
};
constexpr ChipRows kChips[] = { { "2e4", "2e5" }, { "2d4", "2d5" }, { "462", "463" }, { nullptr, "2da" } };

juce::String vc2Setting(State& s, const Blocks::Block& b) {
    const ChipRows& rows = kChips[(size_t)b.kind];
    const juce::String says = Controls::currentText(s, Blocks::blockParam(b, rows.says));
    if (rows.on == nullptr) return "TVA" + kDot + says;
    return Controls::paramValue(s, Blocks::blockParam(b, rows.on)) != 0 ? says : juce::String("OFF");
}

juce::String vocoderWord(State& s) {
    auto* p = dynamic_cast<VSynthEmuProcessor*>(&s.processor);
    return p != nullptr ? Vc2Mic::chipWord(*p) : juce::String();
}

// The path of a zone: its blocks, the effects' switches (MFX, Chorus, Reverb), its Structure, the zone chips and VOCODER
PathBar::Mode pathOf(int zone) {
    const std::array<PathBar::Effect, 3> effects { { { "MFX", vc2HostParamId(0x13d, 0, 0) },
                                                     { "CHORUS", vc2HostParamId(0x13e, 0, 0) },
                                                     { "REVERB", vc2HostParamId(0x13f, 0, 0) } } };
    return { Vc2Blocks::path(zone), vc2Setting, effects, vc2HostParamId(0x2d3, zone, 0),
             vc2HostParamId(kCurrentZone, 0, 0), vocoderWord };
}

}

Vc2SynthPage::Vc2SynthPage(VSynthEmuProcessor& p) : processor_(p), selected_(Blocks::kFirstSelected) {
    refresh();
}

// The path, the rack and the lower panel for a zone, the block and the view kept
void Vc2SynthPage::make(int zone) {
    const LowerPanel::View view = lower_ ? lower_->view() : LowerPanel::View::selected;
    zone_ = zone;
    auto& s = processor_.apvts;
    path_ = std::make_unique<PathBar>(s, pathOf(zone));
    rack_ = std::make_unique<Rack>(Vc2Rack::panels(processor_, zone));
    lower_ = std::make_unique<LowerPanel>(s, Vc2Blocks::path(zone));
    path_->onSelect = [this](int block) { select(block); };
    path_->onEffect = [this](int effect) { if (onEffect) onEffect(effect); };
    path_->onVocoder = [this] { if (onVocoder) onVocoder(); };
    rack_->onSelect = [this](int block) { select(block); };
    lower_->onSelect = [this](int block) { select(block); };
    for (juce::Component* c : std::initializer_list<juce::Component*> { path_.get(), rack_.get(), lower_.get() })
        addAndMakeVisible(c);
    lower_->showView(view);
    select(selected_);
    resized();
}

void Vc2SynthPage::select(int block) {
    selected_ = block;
    path_->setSelected(block);
    rack_->setSelected(block);
    lower_->setSelected(block);
}

void Vc2SynthPage::refresh() {
    const int zone = juce::jlimit(0, Vc2Blocks::kZones - 1,
                                  Controls::paramValue(processor_.apvts, vc2HostParamId(kCurrentZone, 0, 0)));
    if (zone != zone_) make(zone);
    path_->refresh();
    rack_->refresh();
    lower_->refresh();
}

void Vc2SynthPage::resized() {
    auto r = getLocalBounds();
    path_->setBounds(r.removeFromTop(Look::pathH));
    rack_->setBounds(r.removeFromTop(Look::rackH));
    lower_->setBounds(r.removeFromTop(Look::lowerH));
}
