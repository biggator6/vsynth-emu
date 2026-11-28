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
    check(read.frames == kWaveFrames && read.encodeType == 0, "the saved wave isn't the file's frames, LITE");
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

} // namespace

void waveSaveChecks(SynthEditor& ed, VSynthEmuProcessor& p, const juce::File& folder, TestChecks& check) {
    auto* page = pageWithWave(ed, p, folder, kWaveSeconds, check);
    if (page == nullptr) return;
    const juce::File to = folder.getChildFile("saved_wave.wav");
    savesLoops(page->view(0), p, to, check);
    savesCut(*page, page->view(0), p, to, check);
}
