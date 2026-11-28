// VSynthEmuEditorTest's checks of the WAVE page's playhead (EditorTest.h; v485, the user's word of 2026-11-28: a cursor
// following the time while a key is held, to hear the edits). On a SOLO, a LITE and a BACKING wave and a LITE one with
// Vari off (the block's four players: SOLO's, LITE's, the bands', the linear one), a held note's place shows on OSC 1's
// wave, moving on at the wave's rate as the note plays (within half and twice the frames played: a player's place in
// sixteenths read as frames, or the other way, is sixteen times off) and drawn there in the key's Playing colour, and
// OSC 2's, off, shows none; let go and ended, none.
#include "TestMidi.h"
#include "WavePageTest.h"
#include <array>

namespace {

using namespace WavePageTest;
constexpr int kWaveSeconds = 2;
constexpr float kSolo = 1.0f, kBacking = 2.0f, kLite = 4.0f;   // Encode Type's choices
constexpr int kKey = 60;                // the wave's own pitch, so it plays at the wave's rate
constexpr int kHeld = 8, kBlock = 480;  // blocks held before each look, of holdBlocks' size
constexpr int kEnded = 400;             // blocks after the release, for the note to end
constexpr double kRateSlack = 0.5;
constexpr double kWaveRate = 44100.0;   // the XT's, every wave's frames

struct Case {
    float type;
    bool vari;
    const char* says;
};
// Each told by the status the encode leaves, each unlike the one before it (the zoom checks' wave says LITE)
constexpr std::array<Case, 4> kCases { {
    { kSolo, true, "Encoded as SOLO" },
    { kLite, true, "Encoded as LITE" },
    { kBacking, true, "Encoded as BACKING" },
    { kLite, false, "Encoded as LITE" },
} };

void follows(WavePage& page, VSynthEmuProcessor& p, const std::string& kind, TestChecks& check) {
    WaveEditView& v = page.view(0);
    holdBlocks(p, { juce::MidiMessage::noteOn(1, kKey, (juce::uint8)100) }, kHeld - 1);
    page.refresh();
    const long first = v.playing();
    check(first >= 0 && first < v.frames(), kind + ": a held note's place isn't on the wave: " + std::to_string(first));
    const juce::Image shot = v.createComponentSnapshot(v.getLocalBounds());
    const auto at = juce::Point<float>(v.xOf(first), (float)v.getHeight() / 2.0f).toInt();
    check(shot.getPixelAt(at.x, at.y) == WaveEditView::colourOf(WaveEditView::Mark::playing),
          kind + ": the place isn't drawn on the wave in the key's Playing colour");
    holdBlocks(p, {}, kHeld - 1);
    page.refresh();
    const double rate = p.getSampleRate() > 0.0 ? p.getSampleRate() : kWaveRate;
    const double played = kHeld * kBlock * kWaveRate / rate, moved = (double)(v.playing() - first);
    check(moved > played * kRateSlack && moved < played / kRateSlack,
          kind + ": the place moved " + std::to_string(moved) + " frames as " + std::to_string(played) + " played");
    page.showOsc(1);
    page.refresh();
    check(page.view(1).playing() == -1, kind + ": OSC 2, off, shows a place");
    page.showOsc(0);
    holdBlocks(p, { juce::MidiMessage::noteOff(1, kKey) }, kEnded);
    page.refresh();
    check(v.playing() == -1, kind + ": the place still shows, the note ended");
}

} // namespace

void wavePlayChecks(SynthEditor& ed, VSynthEmuProcessor& p, const juce::File& folder, TestChecks& check) {
    const juce::String osc2 = Params::oscId(1, Params::kOscSwitch);
    const float osc2Was = p.apvts.getParameter(osc2)->convertFrom0to1(p.apvts.getParameter(osc2)->getValue());
    setParam(p, osc2, 0.0f);
    setParam(p, Params::kStartOffset, 0.0f);
    for (const Case& c : kCases) {
        const std::string kind = std::string(c.says) + (c.vari ? "" : ", Vari off");
        setParam(p, Params::kVari, c.vari ? 1.0f : 0.0f);
        setParam(p, Params::kEncodeType, c.type);
        auto* page = pageWithWave(ed, p, folder, kWaveSeconds, check);
        if (page == nullptr) return;
        check(comesToSay(p, c.says), kind + ": the wave isn't encoded so: " + p.getStatusText().toStdString());
        follows(*page, p, kind, check);
    }
    setParam(p, Params::kVari, 1.0f);
    setParam(p, Params::kEncodeType, 0.0f);
    setParam(p, osc2, osc2Was);
}
