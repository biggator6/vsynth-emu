// VSynthEmuVc2PlayTest's checks of the part's view (v456; Vc2PartView.h, UiVc2Mic.h): on a processor that has played, a
// note held on the saw, both the mic's and the carrier's checks ticked and the MIC IN card's lines the part's; a second
// and a little more of silence, the mic's check failing and the chip saying NO MIC; the note let go and its release out,
// fewer notes sounding and the carrier's check ticked only while one does (a vocoder patch may keep a carrier sounding).
#include "Vc2PlayRig.h"
#include "../Source/UiVc2Mic.h"

namespace {
constexpr int kSecond = 87, kReleaseOut = 3 * kSecond;   // host blocks of kVc2PlayBlock frames
constexpr int kNote = 60;
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
    blocks(p, kSecond + 2, nullptr, false);
    c = Vc2Mic::checks(p);
    const bool vocoding = c[0].state == State::ok && c[1].state == State::ok;   // the chip's words before NO MIC
    check(c[Vc2Mic::kMicCheck].state == State::silent && (!vocoding || Vc2Mic::chipWord(p) == "NO MIC"),
          name + "a second of silence should fail the mic's check, the chip saying NO MIC with the vocoder on, not " +
              Vc2Mic::chipWord(p).toStdString());
    const int held = state[4].getIntValue();
    blocks(p, 1, &off, false);
    blocks(p, kReleaseOut, nullptr, false);
    const int left = Vc2Mic::stateLines(p)[4].getIntValue();
    const bool carrier = Vc2Mic::checks(p)[Vc2Mic::kCarrierCheck].state == State::ok;
    check(left < held && carrier == (left > 0),
          name + "the note let go and its release out, fewer notes should sound (" + std::to_string(held) +
              " held, " + std::to_string(left) + " after), the carrier's check ticked only while one does");
}
