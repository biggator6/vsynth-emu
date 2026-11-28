// VSynthEmuEditorTest's checks of the WAVE page's Save Wave (EditorTest.h; v489, d-8's file): the shown OSC's wave written
// as the XT writes one, by PluginWaveEdits::save, which the button calls with the file chosen:
// - a plain 2-second file with its forward loop and a backward loop of the session's dragged: written LITE, its
//   frames, the vplp's loops those frames (read back by xtWaveRead), and the status saying it was saved;
// - the same truncated on the page: the kept frames, the file's own samples, and the loops the page holds after it;
// - the saved file loaded back into the OSC plays as many frames.
#include "WavePageTest.h"
#include "../Source/PluginWaveEdit.h"
#include "../Source/XtFiles.h"

namespace {

using namespace WavePageTest;
using Point = WaveEditView::Point;
constexpr int kWaveSeconds = 2;
constexpr long kWaveFrames = kWaveSeconds * 44100L;
constexpr long kFwdStart = 10000, kFwdEnd = 60000, kBwdStart = 20000, kBwdEnd = 50000;
constexpr long kKeepFrom = 22050, kKeepTo = 66149;

// The wave saved to `to`, waited for, and read back; false if the status never said it was saved
bool savedAs(VSynthEmuProcessor& p, const juce::File& to, XtWaveFile& read, std::vector<uint8_t>& bytes) {
    to.deleteFile();
    PluginWaveEdits::save(p, 0, to);
    if (!comesToSay(p, "Saved")) return false;
    juce::MemoryBlock data;
    if (!to.loadFileAsData(data)) return false;
    bytes.assign((const uint8_t*)data.getData(), (const uint8_t*)data.getData() + data.getSize());
    return xtWaveRead(bytes.data(), bytes.size(), read);
}

void savesLoops(WaveEditView& v, VSynthEmuProcessor& p, const juce::File& to, TestChecks& check) {
    dragTo(v, Point::fwdStart, kFwdStart);
    dragTo(v, Point::fwdEnd, kFwdEnd);
    dragTo(v, Point::bwdStart, kBwdStart);
    dragTo(v, Point::bwdEnd, kBwdEnd);
    check(waitFor([&] { return PluginWaveEdits::backward(p, 0).bwdEnd == kBwdEnd; }),
          "the backward loop's edit doesn't reach the wave the synth plays");
    XtWaveFile read;
    std::vector<uint8_t> bytes;
    check(savedAs(p, to, read, bytes), "the wave isn't saved: " + p.getStatusText().toStdString());
    check(read.frames == kWaveFrames && read.encodeType == 0, "the saved wave isn't the file's frames, LITE: " +
                                                                std::to_string(read.frames) + ", type " +
                                                                std::to_string(read.encodeType) + "; " +
                                                                p.getStatusText().toStdString());
    check(read.loopStart == kFwdStart && read.loopEnd == kFwdEnd, "the saved forward loop isn't the page's: " +
                                                                    std::to_string(read.loopStart) + " " +
                                                                    std::to_string(read.loopEnd));
    check(read.bwdStart == kBwdStart && read.bwdEnd == kBwdEnd, "the saved backward loop isn't the session's");
}

void savesCut(WavePage& page, WaveEditView& v, VSynthEmuProcessor& p, const juce::File& to, TestChecks& check) {
    dragTo(v, Point::editStart, kKeepFrom);
    dragTo(v, Point::editEnd, kKeepTo);
    page.truncateButton().onClick();
    const long kept = kKeepTo - kKeepFrom + 1;
    check(waitFor([&] {
              page.refresh();
              return v.frames() == kept;
          }),
          "the page's Truncate didn't keep its frames");
    XtWaveFile read;
    std::vector<uint8_t> bytes;
    check(savedAs(p, to, read, bytes), "the cut wave isn't saved: " + p.getStatusText().toStdString());
    check(read.frames == kept, "the saved cut keeps " + std::to_string(read.frames) + " frames");
    check(read.loopStart == v.at(Point::fwdStart) && read.loopEnd == v.at(Point::fwdEnd),
          "the saved cut's forward loop isn't the page's after it");
    p.loadFile(to, 0);
    check(waitFor([&] {
              page.refresh();
              return v.frames() == kept;
          }),
          "the saved cut loaded back doesn't play its kept frames");
}

// v492: the older XT's SOLO wave played SOLO (Encode Type) and truncated on the page, which the synth then plays as the
// cut encoded SOLO (the XT's Truncate makes it LITE, its Encode SOLO again), saved: SOLO with the kept frames, not its
// own encode's file cut
void savesOwnCut(VSynthEmuProcessor& p, const juce::File& older, TestChecks& check) {
    constexpr float kSoloChoice = 1.0f;
    constexpr int kSolo = 1, kBwdMode = 1;
    setParam(p, Params::kEncodeType, kSoloChoice);
    p.loadFile(older, 0);
    if (!comesToSay(p, "Encoded as SOLO")) return check(false, "the older XT's wave isn't encoded SOLO");
    check(PluginWaveEdits::truncate(p, 0, kKeepFrom, kKeepTo, kBwdMode), "the older XT's wave's Truncate is refused");
    waitFor([&] { return !saysNow(p, "Encoding"); });
    XtWaveFile read;
    std::vector<uint8_t> bytes;
    check(savedAs(p, older.getSiblingFile("saved_own_cut.wav"), read, bytes),
          "the cut older XT's wave isn't saved: " + p.getStatusText().toStdString());
    check(read.encodeType == kSolo && read.frames == kKeepTo - kKeepFrom + 1,
          "the older XT's wave, cut and played SOLO, isn't saved SOLO with the kept frames");
}

// v491: a SOLO wave of an older XT's (the saved one, its LIST's app version "2.01", as the user's project's are) loaded,
// played as its own (Encode Type Auto) and saved: the same file, its own encode kept (made again, it would say this
// image's version)
void keepsOwn(VSynthEmuProcessor& p, std::vector<uint8_t> older, const juce::File& folder, TestChecks& check) {
    constexpr float kAuto = 0.0f;
    const std::string software = "V-Synth Ver.", version = "2.01";
    const auto at = std::search(older.begin(), older.end(), software.begin(), software.end());
    if (at == older.end()) return check(false, "the saved SOLO wave's LIST doesn't name the software");
    std::copy(version.begin(), version.end(), at + (long)software.size());
    const juce::File olderFile = folder.getChildFile("older_solo.wav"), again = folder.getChildFile("saved_again.wav");
    olderFile.replaceWithData(older.data(), older.size());
    setParam(p, Params::kEncodeType, kAuto);
    p.loadFile(olderFile, 0);
    check(comesToSay(p, "(its own)"), "the older XT's wave doesn't play as its own SOLO");
    XtWaveFile read;
    std::vector<uint8_t> bytes;
    check(savedAs(p, again, read, bytes) && bytes == older, "saved as its own, an older XT's encode isn't kept");
    savesOwnCut(p, olderFile, check);
}

// v491: the plain wave encoded SOLO, an event added in its widest gap (where the XT takes one), saved: SOLO, its events
// those the synth plays.  (Its detection finds the same at every depth tried, 0 to 127: the depth given no check here
// can show.)
void savesEncode(SynthEditor& ed, VSynthEmuProcessor& p, const juce::File& folder, TestChecks& check) {
    constexpr float kSoloChoice = 1.0f;
    constexpr int32_t kSixteenths = 16;
    setParam(p, Params::kEncodeType, kSoloChoice);
    auto* page = pageWithWave(ed, p, folder, kWaveSeconds, check);
    if (page == nullptr || !comesToSay(p, "Encoded as SOLO")) return check(false, "the wave isn't encoded SOLO");
    const std::vector<long> was = PluginWaveEdits::events(p, 0);
    if (was.size() < 2) return check(false, "the SOLO wave has no events to add between");
    size_t gap = 0;   // the widest, where the XT takes an event
    for (size_t i = 1; i + 1 < was.size(); ++i) gap = was[i + 1] - was[i] > was[gap + 1] - was[gap] ? i : gap;
    PluginWaveEdits::editEvent(p, 0, { true, (uint32_t)((was[gap] + was[gap + 1]) / 2) });
    waitFor([&] { return !saysNow(p, "Encoding"); });
    const std::vector<long> plays = PluginWaveEdits::events(p, 0);
    check(plays.size() == was.size() + 1, "the event isn't added: " + p.getStatusText().toStdString());
    const juce::File first = folder.getChildFile("saved_solo.wav");
    XtWaveFile read;
    std::vector<uint8_t> bytes;
    check(savedAs(p, first, read, bytes), "the SOLO wave isn't saved: " + p.getStatusText().toStdString());
    check(read.encodeType == 1, "the saved wave isn't SOLO");
    bool same = read.events.size() == plays.size();
    for (size_t i = 0; same && i < plays.size(); ++i) same = read.events[i] / kSixteenths == plays[i];
    check(same, "the saved wave's events aren't those the synth plays: " + std::to_string(read.events.size()) +
                    " against " + std::to_string(plays.size()));
    keepsOwn(p, bytes, folder, check);
}

// v492: a wave encoded ENSEMBLE (Encode Type) and truncated on the page, which the synth plays as the cut encoded
// ENSEMBLE, saved: ENSEMBLE with the kept frames (cut, then encoded, as the XT's screens make them); loaded back, its
// own ENSEMBLE with as many frames.  ENSEMBLE, not BACKING: its status is the longest the header shows
void savesCutBands(SynthEditor& ed, VSynthEmuProcessor& p, const juce::File& folder, TestChecks& check) {
    constexpr float kEnsembleChoice = 3.0f, kAuto = 0.0f;
    constexpr int kEnsemble = 5, kBwdMode = 1;
    setParam(p, Params::kEncodeType, kEnsembleChoice);
    auto* page = pageWithWave(ed, p, folder, kWaveSeconds, check);
    if (page == nullptr || !comesToSay(p, "Encoded as ENSEMBLE"))
        return check(false, "the wave isn't encoded ENSEMBLE");
    check(PluginWaveEdits::truncate(p, 0, kKeepFrom, kKeepTo, kBwdMode), "the page's Truncate is refused");
    waitFor([&] { return !saysNow(p, "Encoding"); });
    const long kept = kKeepTo - kKeepFrom + 1;
    const juce::File to = folder.getChildFile("saved_ensemble.wav");
    XtWaveFile read;
    std::vector<uint8_t> bytes;
    check(savedAs(p, to, read, bytes), "the cut ENSEMBLE wave isn't saved: " + p.getStatusText().toStdString());
    check(read.encodeType == kEnsemble && read.frames == kept, "the saved cut isn't ENSEMBLE with the kept frames");
    setParam(p, Params::kEncodeType, kAuto);
    p.loadFile(to, 0);
    check(comesToSay(p, "(its own)"), "the saved cut doesn't play as its own ENSEMBLE");
    check(waitFor([&] {
              page->refresh();
              return page->view(0).frames() == kept;
          }),
          "the saved cut loaded back doesn't play its kept frames");
}

} // namespace

void waveSaveChecks(SynthEditor& ed, VSynthEmuProcessor& p, const juce::File& folder, TestChecks& check) {
    constexpr float kLiteChoice = 4.0f, kAuto = 0.0f;   // Encode Type's: the loops and the cut on a LITE wave
    setParam(p, Params::kEncodeType, kLiteChoice);
    auto* page = pageWithWave(ed, p, folder, kWaveSeconds, check);
    if (page == nullptr) return;
    check(comesToSay(p, "Encoded as LITE"), "the wave isn't LITE");
    const juce::File to = folder.getChildFile("saved_wave.wav");
    savesLoops(page->view(0), p, to, check);
    savesCut(*page, page->view(0), p, to, check);
    savesEncode(ed, p, folder, check);
    savesCutBands(ed, p, folder, check);
    setParam(p, Params::kEncodeType, kAuto);
}
