// VSynthEmuEditorTest's checks of the WAVE page's events (EditorTest.h; v481, the user's d-9: the XT's own Encode
// screen's add and delete, run by rom.2 itself in the wave's encode; HARDWARE_FIRMWARE.md {#wave-edit-read}):
// - a BACKING wave's events as its encode found them, Next and Prev walking them as the screen's routines do;
// - a detected event deleted at Current, and one added between two, as the wave then plays them;
// - the law of an add, at its edges: 1324 frames before the next event taken and 1323 refused ("Event Interval Too
//   Narrow!"), 221 after the start taken and 220 refused; the start and the end "Cannot Delete This Event!", a frame
//   with none "Event Doesn't Exist Here!"; each refused one dropped from the session and said;
// - a saved session encodes with its edits; another Encode Depth drops them, as the XT's does, on an encode and on an
//   edit;
// - a V-Synth SOLO wave's own events give way to the detection's once it's edited, its edit on SOLO's own encode;
//   Truncate drops the edits with the encode.
// Encode Type and Depth are set before a load, or a session restored after them: a change of either encodes on the
// processor's update from the message loop, which these checks don't run.
#include "TestControls.h"
#include "TestWave.h"
#include "WavePageTest.h"
#include "../Source/PluginParamIds.h"
#include "../Source/PluginWaveEdit.h"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace {

using namespace WavePageTest;
constexpr float kBacking = 2.0f;   // Encode Type's choice
constexpr float kDepth = 100.0f;
constexpr float kOtherDepth = 99.0f;

bool saysNow(VSynthEmuProcessor& p, const char* message) {
    return p.getStatusText().contains(message);
}

// The processor's status come to say `message`, waited for
bool comesToSay(VSynthEmuProcessor& p, const char* message) {
    return waitFor([&] { return saysNow(p, message); });
}

std::string says(VSynthEmuProcessor& p) {
    return ": " + p.getStatusText().toStdString();
}

// An event edited at a frame through the page's buttons, and the encode after it waited for: the edit says
// "Encoding..." at once, and the encode's last act says what it came to (a refusal, or what it encoded as)
void editAt(WavePage& page, VSynthEmuProcessor& p, WavePage::Button b, long frame) {
    page.view(0).setCurrent(frame);
    page.button(b).onClick();
    waitFor([&] { return !saysNow(p, "Encoding"); });
    page.refresh();
}

// 4 s of notes half a second apart, each from near silence (a 220 Hz sine falling to 1/150 by the next): attacks the
// detection finds, where TestWave.h's line of notes, never quieter than 0.6 of its peak, has none at depth 100
juce::File writeNotes(const juce::File& folder) {
    constexpr int kRate = 44100;
    constexpr int kFrames = 4 * kRate;
    juce::AudioBuffer<float> audio(1, kFrames);
    for (int i = 0; i < kFrames; ++i) {
        const double t = (double)i / kRate;
        const double env = std::exp(-10.0 * std::fmod(t, 0.5));
        audio.setSample(0, i, (float)(0.5 * env * std::sin(2.0 * std::numbers::pi * 220.0 * t)));
    }
    const juce::File file = folder.getChildFile("event_notes.wav");
    return writeWav(file, audio, kRate, 16) ? file : juce::File();
}

bool has(const std::vector<long>& events, long frame) {
    return std::find(events.begin(), events.end(), frame) != events.end();
}

// The law at its edges, on an event `at` with room before it, and at the start and the end
void addsAtTheEdges(WavePage& page, VSynthEmuProcessor& p, long at, TestChecks& check) {
    editAt(page, p, WavePage::kAddEvent, at - 1323);
    check(saysNow(p, "Event Interval Too Narrow!") && !has(page.view(0).events(), at - 1323),
          "an event 1323 before the next isn't refused as too narrow" + says(p));
    editAt(page, p, WavePage::kAddEvent, at - 1324);
    check(has(page.view(0).events(), at - 1324), "an event 1324 before the next isn't taken" + says(p));
    editAt(page, p, WavePage::kAddEvent, at - 1324 - 1323);   // the law sees the one just added
    check(saysNow(p, "Event Interval Too Narrow!"), "an event 1323 before an added one isn't refused" + says(p));
    editAt(page, p, WavePage::kAddEvent, 220);
    check(!has(page.view(0).events(), 220), "an event 220 after the start isn't refused");
    editAt(page, p, WavePage::kAddEvent, 221);
    check(has(page.view(0).events(), 221), "an event 221 after the start isn't taken" + says(p));
    editAt(page, p, WavePage::kDeleteEvent, 0);
    check(saysNow(p, "Cannot Delete This Event!"), "the start's delete isn't refused" + says(p));
    editAt(page, p, WavePage::kDeleteEvent, page.view(0).frames() - 1);   // the screen holds it to the end (0x894a9250)
    check(saysNow(p, "Cannot Delete This Event!"), "the end's delete isn't refused" + says(p));
    editAt(page, p, WavePage::kDeleteEvent, 5);
    check(saysNow(p, "Event Doesn't Exist Here!"), "a delete with no event there isn't refused" + says(p));
    const auto kept = PluginWaveEdits::edit(p, 0).events;
    check(kept.size() == 2 && kept[0].at == (uint32_t)(at - 1324) && kept[1].at == 221,
          "the session keeps " + std::to_string(kept.size()) + " edits, not the two taken");
}

// The session encodes with its edits, a detected event's delete `detected` among them; restored at another depth it
// drops them, as an edit at another depth does
void sessionKeepsTheEvents(WavePage& page, VSynthEmuProcessor& p, long at, long detected, TestChecks& check) {
    editAt(page, p, WavePage::kDeleteEvent, detected);
    check(PluginWaveEdits::edit(p, 0).events.size() == 3, "the detected event's delete isn't kept" + says(p));
    check(page.said().contains("3 event edits"), "the page doesn't say the session keeps the edits: " +
                                                     page.said().toStdString());
    VSynthEmuProcessor q;
    const auto restore = [&] {
        juce::MemoryBlock state;
        p.getStateInformation(state);
        q.setStateInformation(state.getData(), (int)state.getSize());
    };
    restore();
    check(PluginWaveEdits::edit(q, 0).events == PluginWaveEdits::edit(p, 0).events, "a session doesn't keep the edits");
    check(waitFor([&] { return PluginWaveEdits::events(q, 0) == PluginWaveEdits::events(p, 0); }),
          "a saved session's wave doesn't play its edited events");
    setParam(p, Params::kEncodeDepth, kOtherDepth);
    restore();
    check(waitFor([&] { return PluginWaveEdits::edit(q, 0).events.empty(); }),
          "a session restored at another depth doesn't drop the edits");
    editAt(page, p, WavePage::kAddEvent, at + (page.view(0).frames() - at) / 2);
    const auto edits = PluginWaveEdits::edit(p, 0).events;
    check(std::none_of(edits.begin(), edits.end(), [](const XtFwEventEdit& e) { return e.at == 221; }),
          "an edit at another depth keeps the ones made at the depth before");
}

// A V-Synth SOLO wave with its own events, Encode Type Auto: its own, until an edit has the detection's list
void editsAnOwnWave(WavePage& page, VSynthEmuProcessor& p, const juce::File& folder, TestChecks& check) {
    constexpr long kOwnFrames = 2 * 44100L;
    constexpr long kOwn = 22050;
    constexpr long kAdded = 30000;
    constexpr int32_t kSixteenths = 16;
    setParam(p, Params::kEncodeType, 0.0f);
    const std::vector<int32_t> own { 0, (int32_t)kOwn * kSixteenths, (int32_t)kOwnFrames * kSixteenths };
    p.loadFile(writeSoloWave(folder, "solo_events.wav", kOwnFrames, own), 0);
    check(comesToSay(p, "Encoded as SOLO"), "the own wave isn't SOLO" + says(p));
    page.refresh();
    check(has(page.view(0).events(), kOwn), "the SOLO wave doesn't play its own events");
    editAt(page, p, WavePage::kAddEvent, kAdded);
    check(has(page.view(0).events(), kAdded), "an event added to a wave with its own doesn't play" + says(p));
    // Truncate drops the encode, and the events edited with it (0x894a9db0)
    check(PluginWaveEdits::truncate(p, 0, 0, kOwnFrames / 2, 1) && PluginWaveEdits::edit(p, 0).events.empty(),
          "Truncate keeps the events edited before it");
    waitFor([&] { return !saysNow(p, "Encoding"); });
}

} // namespace

void waveEventChecks(SynthEditor& ed, VSynthEmuProcessor& p, const juce::File& folder, TestChecks& check) {
    auto* page = shownPage<WavePage>(ed, Blocks::kWavePage, check);
    if (page == nullptr) return;
    page->showOsc(0);
    setParam(p, Params::kEncodeDepth, kDepth);
    setParam(p, Params::kEncodeType, kBacking);
    p.loadFile(writeNotes(folder), 0);
    check(comesToSay(p, "Encoded as BACKING"), "the wave isn't BACKING" + says(p));
    check(waitFor([&] {
              page->refresh();
              return page->view(0).events().size() >= 6;
          }),
          "the BACKING wave's encode found " + std::to_string(page->view(0).events().size()) + " events");
    WaveEditView& v = page->view(0);
    const std::vector<long> found = v.events();
    if (found.size() < 6) return;
    v.setCurrent(0);
    page->button(WavePage::kNextEvent).onClick();
    check(v.at(WaveEditView::Point::current) == found[1], "Next from the start isn't the first attack");
    page->button(WavePage::kPrevEvent).onClick();
    check(v.at(WaveEditView::Point::current) == 0, "Prev from the first attack isn't the start");
    // A detected attack deleted, and one added half way between two
    editAt(*page, p, WavePage::kDeleteEvent, found[2]);
    check(!has(v.events(), found[2]) && has(v.events(), found[3]), "the attack deleted still plays, or another went");
    const long between = (found[3] + found[4]) / 2;
    editAt(*page, p, WavePage::kAddEvent, between);
    check(has(v.events(), between), "an event added between two isn't there" + says(p));
    PluginWaveEdits::forget(p, 0);   // the edges from a fresh list
    addsAtTheEdges(*page, p, found[1], check);
    sessionKeepsTheEvents(*page, p, found[1], found[2], check);
    editsAnOwnWave(*page, p, folder, check);
    for (const auto& id : { Params::kEncodeType, Params::kEncodeDepth }) {   // as the checks after these find them
        auto* prm = p.apvts.getParameter(id);
        prm->setValueNotifyingHost(prm->getDefaultValue());
    }
    p.loadFile(writeTestWave(folder, 2), 0);
    waitFor([&] { return !saysNow(p, "Encoding"); });
}
