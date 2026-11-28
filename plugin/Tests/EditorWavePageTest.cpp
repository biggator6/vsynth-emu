// VSynthEmuEditorTest's checks of the WAVE page (EditorTest.h; v479, research/UI_DESIGN.md "WAVE"; the user's d-8, d-8a):
// - the page shows the chosen OSC's wave in XT samples, its length said;
// - a drag of the Start Offset's line sets the OSC's Start Offset to the sample under the pointer;
// - a drag of either end of the forward loop sets its Loop Start or End so that XtSynth's own reading of them (the
//   controls the processor plays from, as XtSynth::waveLoop takes them) is that sample, on a 2-second wave and every one
//   of a thousand frames of an 8-million-frame one, within a frame on a 16-million-frame one (past 2^23 frames a float
//   share can't hold them all); a loop is held to 16 frames at the least;
// - a drag of the backward loop's end keeps the edit in the session and reaches the wave the synth plays, and a saved
//   session brings it back on its own encode;
// - the wheel narrows the window about the frame under the pointer;
// - the rack's OSC display draws the Start Offset's line while that parameter's gesture runs, and not after.
#include "TestControls.h"
#include "TestWave.h"
#include "WavePageTest.h"
#include "../Source/PluginParamIds.h"
#include "../Source/PluginWaveEdit.h"
#include "../Source/UiOscPanel.h"
#include <random>

namespace {

using namespace WavePageTest;
using Point = WaveEditView::Point;
constexpr int kWaveSeconds = 2;
// A wave as long as every frame holds (under 2^23 frames, 190 s), and one past it, whose ends near its last frame
// XtSynth's share can't all hold: there the nearest
constexpr long kWaveFrames = kWaveSeconds * 44100L, kLongFrames = 8'000'000L, kLongestFrames = 16'000'000L;
constexpr int kTries = 1000;

void dragsStartOffset(WaveEditView& v, VSynthEmuProcessor& p, TestChecks& check) {
    dragTo(v, Point::start, 44100);
    const int now = juce::roundToInt(p.apvts.getRawParameterValue(Params::kStartOffset)->load());
    check(now == 44100, "the Start Offset's line dragged to 44100 sets " + std::to_string(now));
    check(v.at(Point::start) == 44100, "the line isn't drawn where the Start Offset is");
}

void dragsForwardLoop(WaveEditView& v, VSynthEmuProcessor& p, TestChecks& check) {
    dragTo(v, Point::fwdStart, 22050);
    dragTo(v, Point::fwdEnd, 66150);
    const auto c = p.currentControls();
    check(played(c.loopStart, kWaveFrames) == 22050, "the forward loop's start plays at " +
                                                         std::to_string(played(c.loopStart, kWaveFrames)));
    check(played(c.loopEnd, kWaveFrames) == 66150, "the forward loop's end plays at " +
                                                       std::to_string(played(c.loopEnd, kWaveFrames)));
    dragTo(v, Point::fwdEnd, 22050 + 5);
    check(v.at(Point::fwdEnd) == 22050 + kWaveLoopShortest, "the forward loop shorter than 16: its end at " +
                                                                 std::to_string(v.at(Point::fwdEnd)));
    // Every frame of a long wave, through the parameter as a host holds it and back as the processor reads it
    auto* end = dynamic_cast<juce::RangedAudioParameter*>(p.apvts.getParameter(Params::kLoopEnd));
    std::mt19937 rng(478);
    int wrong = 0, far = 0;
    for (int k = 0; end != nullptr && k < kTries; ++k) {
        const int frame = (int)(rng() % (uint32_t)kLongFrames), longest = (int)(rng() % (uint32_t)kLongestFrames);
        end->setValueNotifyingHost(end->convertTo0to1(waveLoopPercentFor(*end, frame, kLongFrames)));
        wrong += played(p.currentControls().loopEnd, kLongFrames) == frame ? 0 : 1;
        end->setValueNotifyingHost(end->convertTo0to1(waveLoopPercentFor(*end, longest, kLongestFrames)));
        far += std::abs(played(p.currentControls().loopEnd, kLongestFrames) - longest) <= 1 ? 0 : 1;
    }
    check(end != nullptr && wrong == 0, std::to_string(wrong) + " of an 8-million-frame wave's frames don't come back");
    check(far == 0, std::to_string(far) + " of a 16-million-frame wave's frames aren't the nearest it can hold");
    if (end != nullptr) end->setValueNotifyingHost(end->convertTo0to1(waveLoopPercentFor(*end, 66150, kWaveFrames)));
}

void dragsBackwardLoop(WaveEditView& v, VSynthEmuProcessor& p, TestChecks& check) {
    dragTo(v, Point::bwdStart, 30000);
    const auto kept = PluginWaveEdits::edit(p, 0);
    check(kept.backward && kept.bwdStart == 30000, "the backward loop's start dragged isn't kept in the session");
    check(waitFor([&] { return PluginWaveEdits::backward(p, 0).bwdStart == 30000; }),
          "the backward loop's edit doesn't reach the wave the synth plays");
    juce::MemoryBlock state;
    p.getStateInformation(state);
    VSynthEmuProcessor q;
    q.setStateInformation(state.getData(), (int)state.getSize());
    check(PluginWaveEdits::edit(q, 0) == kept, "a saved session doesn't keep the backward loop's edit");
    check(waitFor([&] { return PluginWaveEdits::backward(q, 0) == kept; }),
          "a saved session's wave doesn't play its backward loop's edit");
}

void wheelNarrows(WaveEditView& v, TestChecks& check) {
    const auto at = v.handle(Point::start).withX(v.xOf(kWaveFrames / 4));
    const long under = v.frameAt(at.x);
    juce::MouseWheelDetails wheel {};
    wheel.deltaY = 1.0f;
    v.mouseWheelMove(LayoutCheck::mouseAt(v, at, at), wheel);
    check(v.span() < kWaveFrames / 8, "the wheel doesn't narrow the window: " + std::to_string(v.span()));
    check(std::abs(v.frameAt(at.x) - under) <= 1, "the frame under the pointer moved as the window narrowed");
}

void rackDrawsStartLine(SynthEditor& ed, VSynthEmuProcessor& p, TestChecks& check) {
    ed.showPage(Blocks::kSynthPage);
    auto* panel = LayoutCheck::allOf<OscPanel>(ed.drawing()).front();
    auto* start = p.apvts.getParameter(Params::kStartOffset);
    start->beginChangeGesture();
    panel->refresh();
    check(panel->drawsStartLine(), "the rack's display doesn't draw the Start Offset's line while it's held");
    start->endChangeGesture();
    panel->refresh();
    check(!panel->drawsStartLine(), "the rack's display still draws the Start Offset's line once let go");
}

} // namespace

void wavePageChecks(SynthEditor& ed, VSynthEmuProcessor& p, const juce::File& folder, TestChecks& check) {
    auto* page = pageWithWave(ed, p, folder, kWaveSeconds, check);
    if (page == nullptr) return;
    WaveEditView& v = page->view(0);
    check(page->said().contains(juce::String(kWaveFrames) + " samples"), "the wave's line doesn't say its length");
    setParam(p, Params::kLoop, 1.0f);
    dragsStartOffset(v, p, check);
    dragsForwardLoop(v, p, check);
    dragsBackwardLoop(v, p, check);
    wheelNarrows(v, check);
    rackDrawsStartLine(ed, p, check);
}
