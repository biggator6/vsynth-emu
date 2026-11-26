// VSynthEmuVc2PlayTest's checks of the part's view (v456; Vc2PartView.h, UiVc2Mic.h): on a processor that has played, a
// note held on the saw, both the mic's and the carrier's checks ticked and the MIC IN card's lines the part's; a second
// and a little more of silence, the mic's check failing and the chip saying NO MIC; the note let go and its release out,
// fewer notes sounding and the carrier's check ticked only while one does (a vocoder patch may keep a carrier sounding).
// v459: with the note held, the VOCODER card's bands drawn from the view (UiVc2Bands.h).
#include "Vc2PlayRig.h"
#include "../Source/PluginVc2Params.h"
#include "../Source/UiControls.h"
#include "../Source/UiVc2Bands.h"
#include "../Source/UiVc2Mic.h"
#include "../Source/Vc2Params.h"
#include <cstdio>

namespace {
constexpr int kSecond = 87, kReleaseOut = 3 * kSecond;   // host blocks of kVc2PlayBlock frames
constexpr int kNote = 60;
constexpr int kVocoderType = 0x194;   // [FW] Vocoder Type
constexpr juce::uint8 kVelocity = 100;

// `n` host blocks, the saw on the input or silence, `first` (if any) at the first block's start
void blocks(VSynthEmuProcessor& p, int n, const juce::MidiMessage* first, bool input) {
    juce::AudioBuffer<float> buf(2, kVc2PlayBlock);
    for (int b = 0; b < n; ++b) {
        for (int i = 0; i < kVc2PlayBlock; ++i)
            for (int ch = 0; ch < 2; ++ch) buf.setSample(ch, i, input ? vc2PlayInput(b * kVc2PlayBlock + i, ch) : 0.0f);
        juce::MidiBuffer midi;
        if (b == 0 && first != nullptr) midi.addEvent(*first, 0);
        p.processBlock(buf, midi);
    }
}

// v459: the VOCODER card's bands (d-14), drawn from the view with a note held on the saw: the engine's count by the
// patch's Vocoder Sw and Type (LEGACY's 10, HIRESO's 32, none off), and while vocoding the mic's and the output's bars
void bandsChecks(TestChecks& check, VSynthEmuProcessor& p, const std::string& name) {
    constexpr int kSwitch = 0x193, kType = 0x194, kLegacy = 0;   // [FW] Vocoder Sw, Vocoder Type and its LEGACY
    const bool on = Controls::paramValue(p.apvts, vc2HostParamId(kSwitch, 0, 0)) != 0;
    const bool legacy = Controls::paramValue(p.apvts, vc2HostParamId(kType, 0, 0)) == kLegacy;
    const int want = !on ? 0 : legacy ? 10 : 32;
    Vc2BandsView drawn;
    drawn.update(p.vc2Play().view(), 0.0);
    int micTop = 0, outTop = 0;   // each kind's loudest band alone at the top: each on its own scale, none clipped
    for (int b = 0; b < drawn.bands(); ++b) {
        micTop += drawn.micHeight(b) == 1.0f ? 1 : 0;
        outTop += drawn.outHeight(b) == 1.0f ? 1 : 0;
    }
    const bool mic = micTop == 1, out = outTop == 1;
    const bool drawnRight = want == 0 ? drawn.said().contains("OFF") : mic && out && drawn.said().isEmpty();
    check(drawn.bands() == want && drawnRight,
          name + "the VOCODER card should draw " + std::to_string(want) + " bands, the mic's and the output's, not " +
              std::to_string(drawn.bands()) + (mic ? "" : ", no mic") + (out ? "" : ", no output"));
    std::printf("vc2play bands: %s%d bands\n", name.c_str(), drawn.bands());
}
} // namespace

void vc2PlayViewChecks(TestChecks& check, VSynthEmuProcessor& p, const std::string& name) {
    using Vc2Mic::State;
    const juce::MidiMessage on = juce::MidiMessage::noteOn(1, kNote, kVelocity);
    const juce::MidiMessage off = juce::MidiMessage::noteOff(1, kNote);
    blocks(p, kSecond, &on, true);
    auto c = Vc2Mic::checks(p);
    check(c[Vc2Mic::kMicCheck].state == State::ok && c[Vc2Mic::kCarrierCheck].state == State::ok,
          name + "with a note held on the saw, the mic's and the carrier's checks should tick");
    const auto state = Vc2Mic::stateLines(p);
    check(state[0].endsWith("dBFS") && !state[0].startsWith("-inf") && state[4].getIntValue() > 0,
          name + "the MIC IN card should show the input's level and a note sounding, not " + state[0].toStdString());
    bandsChecks(check, p, name);
    blocks(p, kSecond + 2, nullptr, false);
    c = Vc2Mic::checks(p);
    const bool vocoding = c[0].state == State::ok && c[1].state == State::ok;   // the chip's words before NO MIC
    // v460: the vocoder's input is its modulator, the mic as the patch's voices play it or the patch's own wave (018's),
    // so a silent mic silences it only when rom.4's detector finds it quiet (the input stage, 0x899d9a5c); else it
    // vocodes on, the chip saying the Type in rom.4's own text.  The MIC IN card's level the mic's, -inf either way
    const bool quiet = p.vc2Play().player()->part().vocoder.stage.peak() < Vc2PartView::kQuiet;
    const int type = Controls::paramValue(p.apvts, vc2HostParamId(kVocoderType, 0, 0));
    const juce::String chip = Vc2Mic::chipWord(p).trim();
    const juce::String typeText = juce::String(vc2ValueText(kVocoderType, type)).trim();
    const State mic = c[Vc2Mic::kMicCheck].state;
    const bool asRom4 = quiet ? mic == State::silent && (!vocoding || chip == "NO MIC")
                              : mic == State::ok && (!vocoding || chip == typeText);
    check(asRom4 && Vc2Mic::stateLines(p)[0] == "-inf dBFS",
          name + "a second of silence should " + (quiet ? "fail the mic's check, NO MIC" : "leave it vocoding") +
              " with the vocoder on, the MIC IN card's level -inf, not " + chip.toStdString() + ", " +
              Vc2Mic::stateLines(p)[0].toStdString());
    std::printf("vc2play view: %sthe modulator %s in the silence\n", name.c_str(), quiet ? "quiet" : "sounding");
    const int held = state[4].getIntValue();
    blocks(p, 1, &off, false);
    blocks(p, kReleaseOut, nullptr, false);
    const int left = Vc2Mic::stateLines(p)[4].getIntValue();
    const bool carrier = Vc2Mic::checks(p)[Vc2Mic::kCarrierCheck].state == State::ok;
    check(left < held && carrier == (left > 0),
          name + "the note let go and its release out, fewer notes should sound (" + std::to_string(held) +
              " held, " + std::to_string(left) + " after), the carrier's check ticked only while one does");
}
