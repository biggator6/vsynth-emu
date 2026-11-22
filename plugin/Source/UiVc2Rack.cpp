// The VC-2 mode's rack for a zone (UiVc2Rack.h; v453)
#include "UiVc2Rack.h"
#include "PluginVc2Params.h"
#include "UiLook.h"
#include "UiParts.h"
#include "UiVc2Kinds.h"
#include "UiVc2Text.h"
#include <cmath>

namespace Vc2Rack {

namespace {

using Blocks::Kind;
using Controls::KnobRow;
using Display = CosmPanel::Display;
constexpr int kPcm = 1, kExtIn = 2, kVocoder = 3;   // [FW] OSC Type's values: ANALOG 0, PCM 1, EXTIN 2, VOCODER 3
constexpr int kPadMode = 0x4a7, kPadHold = 0x4a8, kXy = 0;   // [FW] Pad Mode X-Y 0, TIME-TRIP 1
constexpr int kPadX = 0x91, kPadY = 0x92, kTripX = 0x93, kTripY = 0x94;   // [FW] the system's assigns
constexpr float kMaxValue = 127.0f;

// Each OSC Type's face, OSC 1's IDs (UI_DESIGN_VC2.md, "An OSC panel"): the box beside Type, four big knobs, six small;
// Vel Sens is the OSC's Amp envelope's Velocity Sens (0x397 + 13·3), as on D
constexpr OscPanel::Face kOscFaces[Vc2Blocks::kOscFaces] = {
    { "2f1", { { { "2e7", "Pitch" }, { "2f3", "Pulse W" }, { "2f2", "Impact" }, { "2ee", "Level" } } },
      { { { "2eb", "Coarse" }, { "2ec", "Fine" }, { "2f4", "PW KF" }, { "2f5", "PW LFO" }, { "2f6", "PW Env" },
          { "3be", "Vel Sens" } } } },
    { "2fe", { { { "2e7", "Pitch" }, { "307", "Time" }, { "30c", "Formant" }, { "2ee", "Level" } } },
      { { { "2eb", "Coarse" }, { "2ec", "Fine" }, { "308", "Time KF" }, { "315", "Energy" }, { "3be", "Vel Sens" },
          { "2e8", "Pitch KF" } } } },
    { "", { { { "2e7", "Pitch" }, { "371", "Harm Fmt" }, { "2eb", "Coarse" }, { "2ee", "Level" } } },
      { { { "2ec", "Fine" }, { "372", "Harm KF" }, { "375", "Harm Style" }, { "2e8", "Pitch KF" }, { "2e6", "Gain" },
          { "3be", "Vel Sens" } } } },
    { "", { { { "2e7", "Pitch" }, { "35f", "Balance" }, { "35b", "Growl" }, { "2ee", "Level" } } },
      { { { "2eb", "Coarse" }, { "2ec", "Fine" }, { "355", "Saw" }, { "354", "Saw LPF" }, { "358", "Noise" },
          { "356", "Noise LP" } } } },
};

// Each COSM Type's face in rom.4's order (UI_DESIGN_VC2.md, "A COSM panel"): its box, up to five knobs, its display; the
// fifteen the VC-2 shares with the V-Synth D's, knob for knob, and the GROWL in the Comb Filter's place
constexpr KnobRow kNone { nullptr, nullptr };
constexpr CosmPanel::Face kCosmFaces[Vc2Blocks::kCosmFaces] = {
    { nullptr, { { kNone, kNone, kNone, kNone, kNone } }, Display::name },
    { "927", { { { "929", "Drive" }, { "92c", "Tone" }, { "928", "Level" }, { "92b", "Env" }, { "92a", "LFO" } } },
      Display::curve },
    { "930", { { { "932", "Drive" }, { "935", "Tone" }, { "931", "Level" }, { "933", "Env" }, { "934", "LFO" } } },
      Display::curve },
    { "939", { { { "93e", "Gain" }, { "93a", "Bass" }, { "93b", "Mid" }, { "93c", "Treble" }, { "93d", "Level" } } },
      Display::curve },
    { "941", { { kNone, kNone, kNone, kNone, kNone } }, Display::name },
    { "942", { { { "943", "Size" }, { "947", "Balance" }, { "944", "KF" }, { "946", "Env" }, { "945", "LFO" } } },
      Display::response },
    { nullptr, { { { "94a", "HPF" }, { "94b", "LPF" }, { "94f", "Detune" }, { "953", "Width" }, { "94c", "KF" } } },
      Display::response },
    { nullptr, { { { "958", "Q" }, { "959", "LPF" }, { "95c", "Detune" }, { "960", "Width" }, { "95a", "KF" } } },
      Display::response },
    { nullptr, { { { "965", "Intensity" }, { "966", "Speed" }, kNone, kNone, kNone } }, Display::wobble },
    { "96c", { { { "96d", "Freq 1" }, { "96f", "Q 1" }, { "972", "Freq 2" }, { "974", "Q 2" }, { "96e", "KF 1" } } },
      Display::response },
    { "977", { { { "97b", "Cutoff" }, { "978", "Reso" }, { "97c", "KF" }, { "97e", "Env" }, { "97d", "LFO" } } },
      Display::response },
    { "97f", { { { "984", "Cutoff" }, { "980", "Reso" }, { "985", "KF" }, { "988", "Env" }, { "986", "LFO" } } },
      Display::response },
    { nullptr, { { { "989", "Attack" }, { "98a", "Sustain" }, { "98b", "Level" }, kNone, kNone } }, Display::name },
    { "98d", { { { "98c", "Thresh" }, { "98e", "Attack" }, { "98f", "Release" }, { "990", "Level" }, kNone } },
      Display::name },
    { nullptr, { { { "991", "Depth" }, { "995", "Balance" }, { "992", "KF" }, { "994", "Env" }, { "993", "LFO" } } },
      Display::name },
    { nullptr, { { { "998", "Bit" }, { "999", "Rate" }, { "99a", "Filter" }, { "99e", "Balance" }, { "99b", "KF" } } },
      Display::response },
    { nullptr, { { { "9a1", "Cutoff" }, { "9a5", "Q" }, { "9a2", "KF" }, { "9a4", "Env" }, { "9ab", "Gain" } } },
      Display::response },
};

// A zone's OSC's parameter's value now, by its OSC 1 row
int zoneOscValue(juce::AudioProcessorValueTreeState& s, int zone, int osc, const char* row) {
    return Controls::paramValue(s, Vc2Blocks::param(zone, Kind::osc, osc, row));
}

// The analog face's picture: D's analog oscillator at the VC-2 OSC's settings, its waveforms the V-Synth's first eleven
WaveDisplay::AnalogSettings analogOf(juce::AudioProcessorValueTreeState& s, int zone, int osc) {
    const auto v = [&](const char* row) { return zoneOscValue(s, zone, osc, row); };
    XtAnalogParams a;
    a.wave = v("2f1");
    a.impact = v("2f2");
    a.pw = v("2f3");
    a.pwKf = v("2f4");
    a.pwLfo = v("2f5");
    a.pwEnv = v("2f6");
    a.fat = v("2f7");
    a.fatKf = v("2f8");
    a.fatLfo = v("2f9");
    a.fatEnv = v("2fa");
    a.detune = v("34d");
    a.subOctave = v("34e");
    a.subLevel = v("34f");
    return { a, v("2e6"), osc != 0 };
}

// A band across the display, filled under its curve: a lowpass at `low` and a highpass at `high` (0…1 across), at a level
void paintBand(juce::Graphics& g, juce::Rectangle<float> r, float high, float low, float level, juce::Colour c) {
    juce::Path band;
    band.startNewSubPath(r.getBottomLeft());
    for (int i = 0; i <= 64; ++i) {
        const float x = (float)i / 64.0f;
        const float lp = 1.0f / (1.0f + std::pow(x / juce::jmax(0.02f, low), 4.0f));
        const float hp = high <= 0.0f ? 1.0f : 1.0f - 1.0f / (1.0f + std::pow(x / high, 4.0f));
        band.lineTo(r.getX() + x * r.getWidth(), r.getBottom() - level * lp * hp * r.getHeight());
    }
    band.lineTo(r.getBottomRight());
    band.closeSubPath();
    Ui::fillPath(g, band, c.withAlpha(0.35f));
    Ui::stroke(g, band, c, juce::PathStrokeType(1.2f));
}

// What the VC-2's OSC display draws that D's doesn't: the VOCODER OSC's carrier, the saw's band by its LPF and level and
// the noise's by its HPF, LPF and level, each by its share of the balance; a PCM wave, by its number, its name and
// picture waiting for the VC-2's wave library; under EXTIN's words, that the mic's cycles wait for Vc2Synth's view
void paintVc2Display(juce::AudioProcessorValueTreeState& s, int zone, juce::Graphics& g, juce::Rectangle<int> r,
                     int osc, int face) {
    const Ui::TextStyle word { Look::sans(10.0f), Look::muted };
    if (face == kVocoder) {
        const auto v = [&](const char* row) { return (float)zoneOscValue(s, zone, osc, row) / kMaxValue; };
        const float noise = v("35f");
        const auto area = r.toFloat();
        paintBand(g, area, 0.0f, v("354"), v("355") * (1.0f - noise), Look::accent);
        paintBand(g, area, v("357"), v("356"), v("358") * noise, Look::mod);
        Ui::drawText(g, "SAW / NOISE", r.removeFromTop(14), word);
        return;
    }
    if (face == kPcm) {
        const juce::String number = Controls::currentText(s, Vc2Blocks::param(zone, Kind::osc, osc, "2fe"));
        Ui::drawText(g, "Wave " + number, r.removeFromTop(16), { Look::sans(11.0f), Look::label });
        Ui::drawText(g, "Its name and picture wait for the VC-2's wave library (step 4.3)", r.removeFromTop(16), word);
        return;
    }
    Ui::drawText(g, "Its cycles wait for the VC-2 synth's view of the mic (step 4.3)", r.removeFromTop(16), word);
}

OscPanel::Model oscModel(VSynthEmuProcessor& p, int zone) {
    OscPanel::Model m;
    m.faces = kOscFaces;
    m.count = Vc2Blocks::kOscFaces;
    m.pcm = kPcm;
    m.extIn = kExtIn;
    m.vocoder = kVocoder;
    m.id = [zone](int osc, const juce::String& row) { return Vc2Blocks::param(zone, Kind::osc, osc, row); };
    m.switchRow = "2e4";
    m.typeRow = "2e5";
    m.line = { "314", "312", "310" };
    m.extInType = vc2HostParamId(0x7a, 0, 0);
    m.loads = false;
    auto& s = p.apvts;
    m.analog = [&s, zone](int osc) { return analogOf(s, zone, osc); };
    m.draw = [&s, zone](juce::Graphics& g, juce::Rectangle<int> r, int osc, int face) {
        paintVc2Display(s, zone, g, r, osc, face);
    };
    return m;
}

// An assign's controller by its text on its string's scale ("CC17" 17), 0 for none (OFF, the bends, aftertouch)
int assignedController(juce::AudioProcessorValueTreeState& s, int id) {
    return juce::jmax(0, Vc2Text::controllerOf(Controls::currentText(s, vc2HostParamId(id, 0, 0))));
}

juce::String ampRow(int zone, const char* row) {
    return Vc2Blocks::param(zone, Kind::amp, 0, row);
}

// AMP's knobs (UI_DESIGN_VC2.md, "AMP"): MFX Send the fifth, the vocoder's level on the VC-2 (d-13)
AmpPanel::Knobs ampKnobs(int zone) {
    AmpPanel::Knobs knobs;
    for (const KnobRow& k : { KnobRow { "2da", "Level" }, KnobRow { "2dd", "Pan" }, KnobRow { "2e0", "MFX" },
                              KnobRow { "2e1", "Cho" }, KnobRow { "2e2", "Rev" } })
        knobs.push_back({ ampRow(zone, k.id), k.name });
    return knobs;
}

}

juce::Point<int> vc2PadAssigns(juce::AudioProcessorValueTreeState& state) {
    const bool xy = Controls::paramValue(state, vc2HostParamId(kPadMode, 0, 0)) == kXy;
    return { assignedController(state, xy ? kPadX : kTripX), assignedController(state, xy ? kPadY : kTripY) };
}

Rack::Panels panels(VSynthEmuProcessor& p, int zone) {
    Rack::Panels made;
    auto& s = p.apvts;
    const auto own = [zone](const char* row) {
        return [zone, row](int osc) { return Vc2Blocks::param(zone, Kind::osc, osc, row); };
    };
    made.trip = std::make_unique<TripColumn>(
        p, TripColumn::Ids { own("316"), own("317"), vc2HostParamId(kPadHold, 0, 0), vc2HostParamId(kPadMode, 0, 0),
                             [&s] { return vc2PadAssigns(s); } });
    for (int osc = 0; osc < Blocks::kOscs; ++osc)
        made.oscs[(size_t)osc] = std::make_unique<OscPanel>(p, osc, oscModel(p, zone));
    const auto cosmId = [zone](int cosm, const juce::String& row) {
        return Vc2Blocks::param(zone, Kind::cosm, cosm, row);
    };
    for (int cosm = 0; cosm < Blocks::kCosms; ++cosm)
        made.cosms[(size_t)cosm] = std::make_unique<CosmPanel>(
            s, cosm, CosmPanel::Model { kCosmFaces, Vc2Blocks::kCosmFaces, cosmId, "462", "463" });
    made.amp = std::make_unique<AmpPanel>(s, ampRow(zone, "2d9"), ampKnobs(zone));
    return made;
}

}
