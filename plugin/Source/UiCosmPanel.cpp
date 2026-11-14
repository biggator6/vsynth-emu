// D's COSM panel (UiCosmPanel.h; UI step 2)
#include "UiCosmPanel.h"
#include "PluginParamIds.h"
#include "UiLook.h"
#include "UiParts.h"
#include <algorithm>
#include <array>

namespace {

using Controls::KnobRow;
constexpr int kTypes = CosmPanel::kTypes;
constexpr int kKnobs = 5, kDial = 30, kTitleH = 20, kDisplayW = 96, kDisplayH = 62;

// How a type's display draws: its response as a filter, its curve as a shaper, or its name
enum class Display { response, curve, name };
// What each type shows: its own box beside Type, its first knobs, its display; IDs as Params::cosmId's names
struct CosmFace {
    const char* sub;
    std::array<KnobRow, kKnobs> knobs;
    Display display;
};
constexpr KnobRow kNone { nullptr, nullptr };
constexpr CosmFace kTvf { "Filter", { { { "Cutoff", "Cutoff" }, { "Reso", "Reso" }, { "CutoffKF", "KF" },
                                        { "CutoffEnv", "Env" }, { "CutoffLfo", "LFO" } } }, Display::response };
constexpr CosmFace kFaces[kTypes] = {
    kTvf,
    kTvf,
    { "DualType", { { { "Dual1Freq", "Freq 1" }, { "Dual1Q", "Q 1" }, { "Dual2Freq", "Freq 2" }, { "Dual2Q", "Q 2" },
                      { "Dual1KF", "KF 1" } } }, Display::response },
    { nullptr, { { { "TbCutoff", "Cutoff" }, { "TbQ", "Q" }, { "TbKF", "KF" }, { "TbEnv", "Env" },
                   { "TbGain", "Gain" } } }, Display::response },
    { "CombOctave", { { { "CombDetune", "Detune" }, { "CombTone", "Tone" }, { "CombDetuneEnv", "Det Env" },
                        { "CombDetuneLfo", "Det LFO" }, { "CombToneEnv", "Tone Env" } } }, Display::response },
    { nullptr, { { { "Sb1Hpf", "HPF" }, { "Sb1Lpf", "LPF" }, { "Sb1Detune", "Detune" }, { "Sb1Width", "Width" },
                   { "Sb1LpfKF", "KF" } } }, Display::response },
    { nullptr, { { { "Sb2Q", "Q" }, { "Sb2Lpf", "LPF" }, { "Sb2Detune", "Detune" }, { "Sb2Width", "Width" },
                   { "Sb2LpfKF", "KF" } } }, Display::response },
    { "ResType", { { { "ResSize", "Size" }, { "ResBalance", "Balance" }, { "ResSizeKF", "KF" }, { "ResSizeEnv", "Env" },
                     { "ResSizeLfo", "LFO" } } }, Display::response },
    { nullptr, { { { "FsDepth", "Depth" }, { "FsBalance", "Balance" }, { "FsDepthKF", "KF" }, { "FsDepthEnv", "Env" },
                   { "FsDepthLfo", "LFO" } } }, Display::name },
    { nullptr, { { { "LofiBit", "Bit" }, { "LofiRate", "Rate" }, { "LofiFilter", "Filter" },
                   { "LofiBalance", "Balance" }, { "LofiFilterKF", "KF" } } }, Display::response },
    { nullptr, { { { "CompAttack", "Attack" }, { "CompSustain", "Sustain" }, { "CompLevel", "Level" }, kNone, kNone } },
      Display::name },
    { "LimRatio", { { { "LimThreshold", "Thresh" }, { "LimAttack", "Attack" }, { "LimRelease", "Release" },
                      { "LimLevel", "Level" }, kNone } }, Display::name },
    { "WsType", { { { "WsDrive", "Drive" }, { "WsTone", "Tone" }, { "WsLevel", "Level" }, { "WsDriveEnv", "Env" },
                    { "WsDriveLfo", "LFO" } } }, Display::curve },
    { "OdType", { { { "OdDrive", "Drive" }, { "OdTone", "Tone" }, { "OdLevel", "Level" }, { "OdDriveEnv", "Env" },
                    { "OdDriveLfo", "LFO" } } }, Display::curve },
    { "AmpType", { { { "AmpGain", "Gain" }, { "AmpBass", "Bass" }, { "AmpMiddle", "Mid" }, { "AmpTreble", "Treble" },
                     { "AmpLevel", "Level" } } }, Display::curve },
    { "SpkType", { { kNone, kNone, kNone, kNone, kNone } }, Display::name },
};

// A filter's response across the display: flat, then falling from the corner, a peak there by the second knob
float responseAt(float x, float corner, float peak) {
    const float f = x / juce::jmax(0.05f, corner);
    return 1.0f / std::sqrt((1.0f - f * f) * (1.0f - f * f) + f * f / (0.3f + 4.0f * (1.0f - peak)));
}

// The display's height at x, 0 to 1, by its kind and the face's first two knobs
float curveAt(Display kind, float x, float a, float b) {
    if (kind == Display::response) return juce::jlimit(0.0f, 1.0f, 0.5f * responseAt(x, 0.1f + 0.85f * a, b));
    return 0.5f + 0.5f * std::tanh((1.0f + 6.0f * a) * (2.0f * x - 1.0f));
}

}

CosmPanel::CosmPanel(juce::AudioProcessorValueTreeState& state, int cosm)
    : FacedPanel(state, { "COSM " + juce::String(cosm + 1), Params::cosmId(cosm, "Switch"),
                          Params::cosmId(cosm, "Type") }, kTypes) {
    for (int t = 0; t < kTypes; ++t) {
        if (kFaces[t].sub != nullptr) addSub(t, Params::cosmId(cosm, kFaces[t].sub));
        for (const KnobRow& k : kFaces[t].knobs)
            if (k.id != nullptr) addKnob(t, Params::cosmId(cosm, k.id), k.name, kDial);
    }
    refresh();
}

int CosmPanel::tableKnobs(int type) {
    const auto& knobs = kFaces[(size_t)juce::jlimit(0, kTypes - 1, type)].knobs;
    return (int)std::count_if(knobs.begin(), knobs.end(), [](const KnobRow& k) { return k.id != nullptr; });
}

float CosmPanel::proportionOf(size_t knob) const {
    const auto& knobs = knobs_[(size_t)juce::jmax(0, face())];
    if (knob >= knobs.size()) return 0.0f;
    auto& dial = knobs[knob]->dial();
    return (float)dial.valueToProportionOfLength(dial.getValue());
}

void CosmPanel::refresh() {
    FacedPanel::refresh();
    juce::String now;
    now << face() << '|' << proportionOf(0) << '|' << proportionOf(1);
    if (now == drawn_) return;
    drawn_ = now;
    repaint(display_);
}

void CosmPanel::resized() {
    auto r = getLocalBounds().reduced(Look::inset, 10);
    layoutTitle(r.removeFromTop(kTitleH), { 72, 128, 98 });
    r.removeFromTop(6);
    display_ = r.removeFromLeft(kDisplayW).withHeight(kDisplayH);
    r.removeFromLeft(8);
    const int w = r.getWidth() / kKnobs;
    for (const auto& face : knobs_) {
        auto row = r;
        for (const auto& k : face) k->setBounds(row.removeFromLeft(w));
    }
}

// A filter's response, a shaper's curve by its first knob, or the type's name
void CosmPanel::paintDisplay(juce::Graphics& g) const {
    Ui::fillRounded(g, display_.toFloat(), Look::field, 6.0f);
    const Display kind = kFaces[(size_t)juce::jmax(0, face())].display;
    if (kind == Display::name) {
        Ui::drawText(g, type_.getText(), display_.reduced(6), { Look::sans(10.0f), Look::muted,
                                                                juce::Justification::centred });
        return;
    }
    const auto area = display_.reduced(6, 8).toFloat();
    const float a = proportionOf(0), b = proportionOf(1);
    const int steps = 96;
    juce::Path path;
    for (int i = 0; i <= steps; ++i) {
        const float x = (float)i / (float)steps;
        const juce::Point<float> pt { area.getX() + x * area.getWidth(),
                                      area.getBottom() - curveAt(kind, x, a, b) * area.getHeight() };
        i == 0 ? path.startNewSubPath(pt) : path.lineTo(pt);
    }
    Ui::stroke(g, path, Look::accent, juce::PathStrokeType(1.8f));
}
