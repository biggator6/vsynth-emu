// VSynthEmuEditorTest's checks of the VC-2 controls' kinds and text and of the mic's checks (EditorTest.h; v453;
// research/UI_DESIGN_VC2.md, d-8, d-9 and "6. The mic's state")
#include "EditorTest.h"
#include "TestControls.h"
#include "../Source/PluginVc2Params.h"
#include "../Source/UiVc2Mic.h"
#include "../Source/UiVc2VocoderPage.h"

namespace {

using Kind = Controls::Kind;

// d-8: a control's kind by rom.4's string, zone 1's
struct KindCase {
    int id;
    Kind kind;
    const char* what;
};
const KindCase kKinds[] = {
    { 0x193, Kind::toggle, "Vocoder Sw (OFF|ON)" },     { 0x13d, Kind::toggle, "MFX Switch (BYPASS|ON)" },
    { 0x19c, Kind::toggle, "Auto Note On (OFF|ON|)" },  { 0x194, Kind::box, "Vocoder Type (LEGACY|HIRESO)" },
    { 0x1a2, Kind::box, "Modulator Key Mode" },         { 0x4a6, Kind::box, "Beam Type (OFF|1~4)" },
    { 0x16a, Kind::box, "a Matrix Source" },            { 0x196, Kind::knob, "Vocoder Sound Level (0~127)" },
    { 0x315, Kind::knob, "OSC 1 Energy (OFF|1~127)" },  { 0x12f, Kind::knob, "Octave Shift (-3~+3)" },
};

// d-9: a value's text in the editor where rom.4 can't write it, on its string's scale; and where it can, rom.4's
struct TextCase {
    int id, value;
    const char* text;
};
const TextCase kTexts[] = {
    { 0x463, 8, "GROWL" }, { 0x463, 0, "THRU" }, { 0x2d3, 0, "1" },       { 0x1a1, 60, "C4" },   { 0x141, 0, "C-1" },
    { 0x1af, 0, "50 Hz" }, { 0x1b0, 1, "0.7" },  { 0x1b1, 15, "0 dB" },   { 0x4a6, 0, "OFF" },   { 0x4a6, 4, "4" },
    { 0x4b2, 0, "1" },     { 0x13a, 1200, "120.0" }, { 0x2f2, 40, "4.0" }, { 0x375, 5, "5" },  { 0x396, 0, "FIXED" },
    { 0x953, 127, "FLAT" }, { 0x130, 1, "POLY" }, { 0x12f, -3, "-3" },   { 0x140, 1, "ZONE2" },
};

// One check failing on its own cause: the parameter set, the checks read, the parameter put back
void failsAlone(VSynthEmuProcessor& p, TestChecks& check, int id, size_t line, const char* word) {
    const juce::String paramId = vc2HostParamId(id, 0, 0);
    const int was = Controls::paramValue(p.apvts, paramId);
    setParam(p, paramId, 0.0f);
    const auto checks = Vc2Mic::checks(p);
    for (size_t i = 0; i < checks.size(); ++i) {
        const bool silent = checks[i].state == Vc2Mic::State::silent;
        check(silent == (i == line), ("with " + paramId + " at 0, the check " + checks[i].name + " is " +
                                      (silent ? "silent" : "not silent")).toStdString());
    }
    const std::string at = (" with " + paramId + " at 0").toStdString();
    check(Vc2Mic::status(p) == checks[line].why, "the status doesn't say why," + at);
    check(Vc2Mic::chipWord(p) == word, std::string("the VOCODER chip should say ") + word + "," + at);
    setParam(p, paramId, (float)was);
}

}

void vc2TextChecks(SynthEditor& ed, VSynthEmuProcessor& p, TestChecks& check) {
    for (const KindCase& k : kKinds)
        check(Controls::kindFor(p.apvts, vc2HostParamId(k.id, 0, 0)) == k.kind,
              std::string(k.what) + " isn't the control rom.4's string makes it");
    for (const TextCase& t : kTexts) {
        const juce::String said = Controls::editorValueText(p.apvts, vc2HostParamId(t.id, 0, 0), t.value);
        check(said == t.text, ("0x" + juce::String::toHexString(t.id) + " at " + juce::String(t.value) + " reads \"" +
                               said + "\", not " + t.text).toStdString());
    }
    auto* tempo = p.apvts.getParameter(vc2HostParamId(0x13a, 0, 0));
    check(tempo->getText(tempo->convertTo0to1(1200.0f), 0) == "1200", "the host's text should stay rom.4's routine's");
    // The mic's checks: each failing on its own, the status and the path's chip saying so; the host's input off
    const auto all = Vc2Mic::checks(p);
    check(all.size() == 5 && Vc2Mic::heard(p), "with the patch's defaults the vocoder should be heard, as far as seen");
    failsAlone(p, check, 0x193, 0, "OFF");
    failsAlone(p, check, 0x196, 4, "LEVEL 0");
    failsAlone(p, check, 0x2e0, 4, "LEVEL 0");
    const auto layout = p.getBusesLayout();
    auto off = layout;
    off.inputBuses.getReference(0) = juce::AudioChannelSet::disabled();
    check(p.setBusesLayout(off), "the input bus won't go off");
    check(Vc2Mic::checks(p)[1].state == Vc2Mic::State::silent && Vc2Mic::chipWord(p) == "NO INPUT",
          "with no input bus the Input check should fail, and the chip say NO INPUT");
    if (auto* page = dynamic_cast<Vc2VocoderPage*>(ed.pageView(Blocks::kVc2VocoderPage))) {
        page->refresh();
        check(page->status() == Vc2Mic::checks(p)[1].why, "the MIC IN card's status doesn't say the input is off");
    }
    p.setBusesLayout(layout);
    for (const Vc2Mic::Check& c : Vc2Mic::checks(p))
        if (c.state == Vc2Mic::State::waits) check(c.why.contains("step 4.3"), "a waiting check should say what for");
}
