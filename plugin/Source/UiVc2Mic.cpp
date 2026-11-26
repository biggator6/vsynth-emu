// The mic's checks (UiVc2Mic.h; v453; v456 the part's view)
#include "UiVc2Mic.h"
#include "PluginVc2Params.h"
#include "UiControls.h"
#include <cmath>

namespace Vc2Mic {

namespace {

// [FW] Vocoder Sw, Vocoder Type, Vocoder Sound Level; zone 1's MFX Send, which takes the vocoder's output to the MFX
constexpr int kSwitch = 0x193, kType = 0x194, kSound = 0x196, kMfxSend = 0x2e0;
constexpr double kPartRate = 44100.0;   // [FW] the part's rate: a tracked period in its samples
const char* const kWaits = "waits for the VC-2 part, made once the VC-2 mode plays";
const char* const kLineWaits = "waits for the VC-2 part";   // a state line's, in its narrower box
const char* const kQuiet = "The input is quiet on the channels External In Type takes (under -60 dBFS for a second)";
const char* const kNoNote = "No note sounds: hold a key, set Modulator Key Mode ALWAYS-ON, or turn Auto Note on";

int patchValue(VSynthEmuProcessor& p, int id) {
    return Controls::paramValue(p.apvts, vc2HostParamId(id, 0, 0));
}

// A check the part's view decides: waiting while no part has written it, else ok or silent
State byView(const Vc2PartView& v, bool ok) {
    if (!v.playing.load(std::memory_order_relaxed)) return State::waits;
    return ok ? State::ok : State::silent;
}

juce::String dbText(float peak) {
    return peak > 0.0f ? juce::String(20.0 * std::log10((double)peak), 1) + " dBFS" : juce::String("-inf dBFS");
}

juce::String noteText(int note) { return juce::MidiMessage::getMidiNoteName(note, true, true, 4); }

}

std::vector<Check> checks(VSynthEmuProcessor& p) {
    const Vc2PartView& v = p.vc2Play().view();
    const bool on = patchValue(p, kSwitch) != 0;
    const bool level = patchValue(p, kSound) != 0 && patchValue(p, kMfxSend) != 0;
    const juce::String low = patchValue(p, kSound) == 0 ? "Vocoder Sound Level is 0" : "Zone 1's MFX Send is 0";
    const bool playing = v.playing.load(std::memory_order_relaxed);
    const bool mic = v.quietFrames.load(std::memory_order_relaxed) < Vc2PartView::kQuietFrames;
    const bool note = v.sounding.load(std::memory_order_relaxed) > 0;
    return {
        { "Vocoder", on ? State::ok : State::silent, "Vocoder Sw is OFF: only the Direct Sound passes" },
        { "Input", p.hasInput() ? State::ok : State::silent, "The host gives the plugin no input bus" },
        { "Mic signal", byView(v, mic), playing ? juce::String(kQuiet) : juce::String("The input's level ") + kWaits },
        { "Carrier", byView(v, note), playing ? juce::String(kNoNote) : juce::String("The notes sounding ") + kWaits },
        { "Level", level ? State::ok : State::silent, low },
    };
}

std::vector<juce::String> stateLines(VSynthEmuProcessor& p) {
    const Vc2PartView& v = p.vc2Play().view();
    constexpr auto relaxed = std::memory_order_relaxed;
    if (!v.playing.load(relaxed)) return std::vector<juce::String>(kStateLines, kLineWaits);
    const float period = v.period.load(relaxed);
    const int tracked = period > 0.0f ? (int)std::lround(69.0 + 12.0 * std::log2(kPartRate / period / 440.0)) : -1;
    const int autoNote = v.autoNote.load(relaxed);
    return {
        dbText(v.inputPeak.load(relaxed)),
        v.voiced.load(relaxed) ? "VOICED" : "UNVOICED",
        tracked >= 0 ? noteText(tracked) + ", lock " + juce::String(v.lock.load(relaxed)) : juce::String("none"),
        autoNote >= 0 ? noteText(autoNote) : juce::String("none"),
        juce::String(v.sounding.load(relaxed)),
        dbText(v.outputPeak.load(relaxed)),
    };
}

juce::String status(VSynthEmuProcessor& p) {
    bool waiting = false;
    for (const Check& c : checks(p)) {
        if (c.state == State::silent) return c.why;
        waiting = waiting || c.state == State::waits;
    }
    const juce::String type = Controls::currentText(p.apvts, vc2HostParamId(kType, 0, 0));
    return waiting ? "Vocoding, " + type + ", as far as the editor sees: the mic's signal and a note " + kWaits
                   : "Vocoding, " + type;
}

juce::String chipWord(VSynthEmuProcessor& p) {
    if (patchValue(p, kSwitch) == 0) return "OFF";
    if (!p.hasInput()) return "NO INPUT";
    const std::vector<Check> c = checks(p);
    if (c[kMicCheck].state == State::silent) return "NO MIC";
    if (c[kCarrierCheck].state == State::silent) return "NO NOTE";
    if (patchValue(p, kSound) == 0 || patchValue(p, kMfxSend) == 0) return "LEVEL 0";
    return Controls::currentText(p.apvts, vc2HostParamId(kType, 0, 0));
}

bool heard(VSynthEmuProcessor& p) {
    for (const Check& c : checks(p))
        if (c.state == State::silent) return false;
    return true;
}

}
