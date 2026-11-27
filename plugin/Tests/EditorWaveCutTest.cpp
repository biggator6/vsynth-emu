// VSynthEmuEditorTest's checks of the WAVE page's Truncate (EditorTest.h; v480, the user's d-8a, the XT's own law in
// HARDWARE_FIRMWARE.md {#wave-truncate-read}):
// - the Edit region's ends dragged in the ruler and Truncate pressed keep those frames, both ends kept: the wave as the
//   synth plays it is that many frames, its samples the kept ones; the forward loop, strictly inside, moves with them;
//   the backward loop is the whole wave;
// - a cut of a cut is of the file's frames, and the forward loop not strictly inside becomes the whole wave;
// - a saved session plays the cut on its own encode; Truncate on an empty region does nothing; Revert is the file's;
// - a 48 kHz file is cut in the XT's frames, on the XT's own import to 44.1 kHz;
// - a V-Synth SOLO wave cut is LITE, as the XT's Truncate leaves it.
#include "TestControls.h"
#include "TestWave.h"
#include "WavePageTest.h"
#include "../Source/PluginParamIds.h"
#include "../Source/PluginWaveEdit.h"

namespace {

using namespace WavePageTest;
using Point = WaveEditView::Point;
constexpr long kFrames = 2 * 44100L;

// An OSC's forward loop put on its parameters, its ends in frames of a wave of `frames`
void putLoopFrames(VSynthEmuProcessor& p, int osc, long start, long end, long frames) {
    for (const auto& [id, f] : { std::pair { Params::kLoopStart, start }, std::pair { Params::kLoopEnd, end } }) {
        auto* r = dynamic_cast<juce::RangedAudioParameter*>(p.apvts.getParameter(Params::oscId(osc, id)));
        r->setValueNotifyingHost(r->convertTo0to1(waveLoopPercentFor(*r, (int)f, frames)));
    }
}

// The wave's frames as the page draws it, once the encode has published it
bool framesAre(WavePage& page, int osc, long frames) {
    return waitFor([&] {
        page.refresh();
        return page.view(osc).frames() == frames;
    });
}

std::vector<float> monoOf(VSynthEmuProcessor& p, int osc) {
    const auto view = p.waveView(osc);
    return view ? view->mono : std::vector<float> {};
}

// `after` is `before` from `from` on, sample for sample
bool keeps(const std::vector<float>& before, const std::vector<float>& after, long from) {
    if (after.empty() || (long)before.size() < from + (long)after.size()) return false;
    return std::equal(after.begin(), after.end(), before.begin() + from);
}

void cutsTheWave(WavePage& page, VSynthEmuProcessor& p, TestChecks& check) {
    WaveEditView& v = page.view(0);
    page.revertButton().onClick();
    check(framesAre(page, 0, kFrames), "the wave reverted isn't its file's 2 seconds");
    const std::vector<float> before = monoOf(p, 0);
    putLoopFrames(p, 0, 30000, 40000, kFrames);
    dragTo(v, Point::editEnd, 66149);
    dragTo(v, Point::editStart, 22050);
    page.truncateButton().onClick();
    check(framesAre(page, 0, 44100), "Truncate on 22050..66149 doesn't keep 44100 frames: " +
                                         std::to_string(v.frames()));
    const PluginWaveEdit kept = PluginWaveEdits::edit(p, 0);
    check(kept.cut && kept.keepFrom == 22050 && kept.keepTo == 66149, "the session doesn't keep the cut");
    check(keeps(before, monoOf(p, 0), 22050), "the cut's samples aren't the file's from 22050");
    const auto c = p.currentControls();
    check(played(c.loopStart, 44100) == 7950 && played(c.loopEnd, 44100) == 17950,
          "the forward loop inside the cut doesn't move with it: " + std::to_string(played(c.loopStart, 44100)));
    const PluginWaveEdit bwd = PluginWaveEdits::backward(p, 0);
    check(bwd.backward && bwd.bwdStart == 0 && bwd.bwdEnd == 44099, "the backward loop isn't the whole cut wave");
    // A cut of the cut: the file's frames 32050..52050; the loop not strictly inside it the whole wave
    dragTo(v, Point::editEnd, 30000);
    dragTo(v, Point::editStart, 10000);
    page.truncateButton().onClick();
    check(framesAre(page, 0, 20001), "a cut of the cut doesn't keep 20001 frames");
    const PluginWaveEdit twice = PluginWaveEdits::edit(p, 0);
    check(twice.keepFrom == 32050 && twice.keepTo == 52050, "a cut of the cut isn't of the file's frames");
    check(keeps(before, monoOf(p, 0), 32050), "a cut of the cut's samples aren't the file's from 32050");
    const auto c2 = p.currentControls();
    check(played(c2.loopStart, 20001) == 0 && played(c2.loopEnd, 20001) == 20000,
          "a forward loop not strictly inside the cut isn't the whole wave");
    dragTo(v, Point::editEnd, v.at(Point::editStart));
    check(!v.truncate() && PluginWaveEdits::edit(p, 0) == twice, "Truncate on an empty region did something");
    check(!PluginWaveEdits::truncate(p, 0, 100, 100, 1) && PluginWaveEdits::edit(p, 0) == twice,
          "Truncate on one frame did something, where the XT's refuses");
    dragTo(v, Point::editEnd, 15000);   // the two ends apart again (a press on both takes the end)
    dragTo(v, Point::editStart, 1000);
    dragTo(v, Point::editEnd, 900);
    check(v.at(Point::editEnd) == 1000, "the Edit region's end passes its start");
    // A loop end on an edit point isn't strictly inside: the whole wave (0x893810f0)
    const std::pair<long, long> whole { 0, 900 };
    check(waveLoopAfterCut(100, 500, 100, 1000) == whole && waveLoopAfterCut(200, 1000, 100, 1000) == whole,
          "a loop with an end on the region's end isn't the whole wave after the cut");
    // A backward loop dragged after a cut keeps the cut: the session's edit is both
    dragTo(v, Point::bwdStart, 5000);
    check(waitFor([&] { return PluginWaveEdits::backward(p, 0).bwdStart == 5000; }),
          "a backward loop dragged after a cut doesn't reach the wave");
    check(PluginWaveEdits::edit(p, 0).cut && framesAre(page, 0, 20001), "a backward loop dragged after a cut loses it");
}

void sessionKeepsTheCut(VSynthEmuProcessor& p, TestChecks& check) {
    juce::MemoryBlock state;
    p.getStateInformation(state);
    VSynthEmuProcessor q;
    q.setStateInformation(state.getData(), (int)state.getSize());
    check(PluginWaveEdits::edit(q, 0) == PluginWaveEdits::edit(p, 0), "a saved session doesn't keep the cut");
    check(waitFor([&] {
              const auto view = q.waveView(0);
              return view && view->frames == 20001;
          }),
          "a saved session's wave isn't cut on its own encode");
}

void cutsOnTheImport(WavePage& page, VSynthEmuProcessor& p, const juce::File& folder, TestChecks& check) {
    juce::AudioBuffer<float> audio(1, 48000);
    for (int i = 0; i < audio.getNumSamples(); ++i) audio.setSample(0, i, 0.4f * std::sin(0.05f * (float)i));
    const juce::File file = folder.getChildFile("cut_48k.wav");
    check(writeWav(file, audio, 48000, 16), "the 48 kHz wave couldn't be written");
    setParam(p, Params::oscId(1, Params::kOscType), 1.0f);
    p.loadFile(file, 1);
    page.showOsc(1);
    check(waitFor([&] { return !p.getStatusText().contains("Encoding"); }), "the 48 kHz wave doesn't encode");
    const std::vector<std::vector<float>> loaded { monoOf(p, 1) };   // its frames as read from the file, at 48 kHz
    check(PluginWaveEdits::truncate(p, 1, 1000, 20000, 1), "Truncate on the 48 kHz wave refused");
    check(framesAre(page, 1, 19001), "the 48 kHz wave's cut isn't 19001 of the XT's frames");
    std::vector<float> imported;
    for (int16_t s : xtSynthImport(loaded, 48000.0, 16)) imported.push_back((float)s / 32768.0f);
    check(keeps(imported, monoOf(p, 1), 1000), "the 48 kHz wave isn't cut on the XT's import to 44.1 kHz");
    // A V-Synth SOLO wave, cut, is LITE (vpsc's type 0 after Truncate, 0x894a2270)
    p.loadFile(writeSoloWave(folder, "solo_cut.wav", kFrames), 1);
    check(waitFor([&] { return p.getStatusText().contains("Encoded as SOLO"); }), "the SOLO wave isn't its own SOLO");
    check(PluginWaveEdits::truncate(p, 1, 0, 44099, 1), "Truncate on the SOLO wave refused");
    check(waitFor([&] { return p.getStatusText().contains("Encoded as LITE"); }),
          "a SOLO wave cut isn't LITE: " + p.getStatusText().toStdString());
}

} // namespace

void waveCutChecks(SynthEditor& ed, VSynthEmuProcessor& p, const juce::File& folder, TestChecks& check) {
    auto* page = shownPage<WavePage>(ed, Blocks::kWavePage, check);
    if (page == nullptr) return;
    page->showOsc(0);
    cutsTheWave(*page, p, check);
    sessionKeepsTheCut(p, check);
    cutsOnTheImport(*page, p, folder, check);
    page->showOsc(0);
    page->revertButton().onClick();
    check(framesAre(*page, 0, kFrames) && PluginWaveEdits::edit(p, 0) == PluginWaveEdit {},
          "Revert isn't the wave as its file has it");
    auto* type2 = p.apvts.getParameter(Params::oscId(1, Params::kOscType));   // OSC 2 as the checks after these find it
    type2->setValueNotifyingHost(type2->getDefaultValue());
}
