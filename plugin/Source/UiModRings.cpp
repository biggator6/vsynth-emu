// The knobs' modulation rings (UiModRings.h, v464)
#include "UiModRings.h"
#include "PluginParamIds.h"
#include "PluginProcessor.h"
#include "UiKnob.h"
#include "UiLook.h"
#include "UiMidiLearn.h"

namespace {
// Whether a knob's dial already shows the ring from lo to hi (none when they're the same)
bool ringIs(juce::Slider& dial, float lo, float hi) {
    const auto& props = dial.getProperties();
    if (juce::approximatelyEqual(lo, hi)) return !props.contains(Look::modLo);
    if (!props.contains(Look::modLo)) return false;
    const float was = props[Look::modLo], wasHi = props[Look::modHi];
    return juce::approximatelyEqual(was, juce::jmin(lo, hi)) && juce::approximatelyEqual(wasHi, juce::jmax(lo, hi));
}
}  // namespace

ModRings::ModRings(VSynthEmuProcessor& p) : processor_(p) {
    using namespace xtmx;
    for (int k = 0; k < 2; ++k) {   // each OSC: its Level, its LFO's Rate and depths (XtSynthKnobs.cpp, XtSynthEnvKnobs.cpp)
        const int dO = k ? Osc2 : 0, dL = k ? Osc2Lfo : OscLfoRate, os = k ? xtmsm::Osc2 : xtmsm::Osc1;
        reach_[Params::oscId(k, Params::kOscLevel)] = { OscLevel + dO, os + xtmsm::OscLevel };
        reach_[Params::oscId(k, Params::kLfoRate)] = { dL, os + xtmsm::OscLfoRate };
        reach_[Params::oscId(k, Params::kLfoDepth)] = { dL + 1, os + xtmsm::OscLfoPitch };
        reach_[Params::oscId(k, Params::kLfoTimeDepth)] = { dL + 2, os + xtmsm::OscLfoTime };
        reach_[Params::oscId(k, Params::kLfoFmtDepth)] = { dL + 3, os + xtmsm::OscLfoFormant };
        reach_[Params::oscId(k, Params::kOscLevelLfo)] = { dL + 4, os + xtmsm::OscLfoLevel };
        reach_[Params::oscId(k, Params::kAnaPwLfo)] = { dL + 2, os + xtmsm::OscLfoTime };   // the analog's, on time's
        reach_[Params::oscId(k, Params::kAnaFatLfo)] = { dL + 3, os + xtmsm::OscLfoFormant };
        reach_[Params::cosmId(k, "LfoRate")] = { (k ? Cosm2 : Cosm) + CosmLfoRate,
                                                 (k ? xtmsm::Cosm2 : xtmsm::Cosm1) + xtmsm::CosmLfoRate };
    }
    const int tva = xtmsm::Tva;
    reach_[Params::kTvaLevel] = { TvaLevel, tva + xtmsm::TvaLevel };
    reach_[Params::kTvaPan] = { TvaPan, tva + xtmsm::TvaPan };
    reach_[Params::kTvaLevelLfo] = { TvaLfoLevel, tva + xtmsm::TvaLfoLevel };
    reach_[Params::kTvaPanLfo] = { TvaLfoPan, tva + xtmsm::TvaLfoPan };
    reach_[Params::kTvaLfoRate] = { TvaLfoRate, tva + xtmsm::TvaLfoRate };
    reach_[Params::kMfxSend] = { MfxSend, xtmsm::MfxSend };
    reach_[Params::kTvaChoSend] = { ChorusSend, xtmsm::ChoSend };
    reach_[Params::kTvaRevSend] = { ReverbSend, xtmsm::RevSend };
    reach_[Params::kPortaTime] = { -1, xtmsm::PortaTime };   // the MSM's alone (XtSynthKnobs.cpp, portaSettings)
}

int ModRings::reachOf(const juce::String& id) const {
    const XtSynthView& view = processor_.modulation();
    const auto at = reach_.find(id);
    if (at == reach_.end() || !view.sounding()) return 0;
    const Reach& r = at->second;
    return (r.slot >= 0 ? view.matrix(r.slot) : 0) + (r.dest >= 0 ? view.msm(r.dest) : 0);
}

void ModRings::show(juce::Component& root) {
    for (juce::Component* c : root.getChildren()) {
        if (!c->isVisible()) continue;
        auto* knob = dynamic_cast<Knob*>(c);
        if (knob == nullptr) {
            show(*c);
            continue;
        }
        const juce::String id = MidiLearn::learnsFor(knob->dial());
        auto* param = dynamic_cast<juce::RangedAudioParameter*>(processor_.apvts.getParameter(id));
        if (param == nullptr || !rings(id)) continue;
        const float stored = param->convertFrom0to1(param->getValue());
        const auto& range = param->getNormalisableRange();
        const float to = juce::jlimit(range.start, range.end, stored + (float)reachOf(id));
        const float lo = param->convertTo0to1(stored), hi = param->convertTo0to1(to);
        if (!ringIs(knob->dial(), lo, hi)) knob->setModulation(lo, hi);
    }
}
