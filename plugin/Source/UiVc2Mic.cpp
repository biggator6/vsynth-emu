// The mic's checks (UiVc2Mic.h; v453)
#include "UiVc2Mic.h"
#include "PluginVc2Params.h"
#include "UiControls.h"

namespace Vc2Mic {

namespace {

// [FW] Vocoder Sw, Vocoder Type, Vocoder Sound Level; zone 1's MFX Send, which takes the vocoder's output to the MFX
constexpr int kSwitch = 0x193, kType = 0x194, kSound = 0x196, kMfxSend = 0x2e0;
const char* const kWaits = "waits for the VC-2 synth's view of its state (step 4.3)";

int patchValue(VSynthEmuProcessor& p, int id) {
    return Controls::paramValue(p.apvts, vc2HostParamId(id, 0, 0));
}

}

std::vector<Check> checks(VSynthEmuProcessor& p) {
    const bool on = patchValue(p, kSwitch) != 0;
    const bool level = patchValue(p, kSound) != 0 && patchValue(p, kMfxSend) != 0;
    const juce::String low = patchValue(p, kSound) == 0 ? "Vocoder Sound Level is 0" : "Zone 1's MFX Send is 0";
    return {
        { "Vocoder", on ? State::ok : State::silent, "Vocoder Sw is OFF: only the Direct Sound passes" },
        { "Input", p.hasInput() ? State::ok : State::silent, "The host gives the plugin no input bus" },
        { "Mic signal", State::waits, juce::String("The input's level ") + kWaits },
        { "Carrier", State::waits, juce::String("The notes sounding ") + kWaits },
        { "Level", level ? State::ok : State::silent, low },
    };
}

juce::String status(VSynthEmuProcessor& p) {
    for (const Check& c : checks(p))
        if (c.state == State::silent) return c.why;
    return "Vocoding, " + Controls::currentText(p.apvts, vc2HostParamId(kType, 0, 0)) +
           ", as far as the editor sees: the mic's signal and a note " + kWaits;
}

juce::String chipWord(VSynthEmuProcessor& p) {
    if (patchValue(p, kSwitch) == 0) return "OFF";
    if (!p.hasInput()) return "NO INPUT";
    if (patchValue(p, kSound) == 0 || patchValue(p, kMfxSend) == 0) return "LEVEL 0";
    return Controls::currentText(p.apvts, vc2HostParamId(kType, 0, 0));
}

bool heard(VSynthEmuProcessor& p) {
    for (const Check& c : checks(p))
        if (c.state == State::silent) return false;
    return true;
}

}
