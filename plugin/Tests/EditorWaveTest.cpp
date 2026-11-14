// VSynthEmuEditorTest's checks of the OSC displays (EditorTest.h; v200): an analog OSC draws its oscillator's own cycles
// at the panel's settings, made again as a setting moves, each waveform its own picture; a PCM OSC draws its loaded wave,
// whose view holds the file's own lowest and highest samples, and a newer wave's when one comes.
#include "EditorTest.h"
#include "TestControls.h"
#include "TestWave.h"
#include "../Source/PluginParamIds.h"
#include "../Source/UiOscPanel.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <algorithm>

namespace {

constexpr float kApart = 0.05f;                    // two pictures differ by more than this somewhere
constexpr int kWaveSeconds = 2;
constexpr float kAnalog = 0.0f, kPcm = 1.0f;       // OSC Type's values
constexpr float kSquare = 1.0f, kNarrow = 40.0f;   // Analog Waveform SQUARE, and a Pulse Width far from its 0
// X-MOD has no depth on OSC 2 (xtAnalogBlock), where it plays, and draws, as SAW: the one pair alike there, measured
constexpr int kAlikeOnOsc2 = 1;

float largestGap(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.size() != b.size()) return 1.0f;
    float gap = 0.0f;
    for (size_t i = 0; i < a.size(); ++i) gap = std::max(gap, std::abs(a[i] - b[i]));
    return gap;
}

// The file's own lowest and highest sample, read here, apart from the processor's reading
juce::Range<float> fileRange(const juce::File& file) {
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if (!reader) return {};
    juce::AudioBuffer<float> audio((int)reader->numChannels, (int)reader->lengthInSamples);
    reader->read(&audio, 0, audio.getNumSamples(), 0, true, true);
    return juce::FloatVectorOperations::findMinAndMax(audio.getReadPointer(0), audio.getNumSamples());
}

// Each Analog Waveform on the panel: the picture is made again as the parameter moves (it had stayed as first drawn),
// as the oscillator's own at the settings as they stand; how many waveforms draw alike is said; the Pulse Width moves a
// square's picture
void analogPictures(OscPanel& panel, VSynthEmuProcessor& p, int osc, TestChecks& check) {
    const juce::String waveId = Params::oscId(osc, Params::kAnaWave), pwId = Params::oscId(osc, "anaPw");
    setParam(p, Params::oscId(osc, Params::kOscType), kAnalog);
    const int waves = p.apvts.getParameter(waveId)->getNumSteps();
    std::vector<std::vector<float>> seen;
    for (int w = 0; w < waves; ++w) {
        setParam(p, waveId, (float)w);
        panel.refresh();
        const auto fresh = WaveDisplay::analogCycles(WaveDisplay::analogSettings(p.currentControls(), osc));
        const std::string what = p.apvts.getParameter(waveId)->getCurrentValueAsText().toStdString();
        check(!panel.cycles().empty(), what + " draws nothing");
        check(largestGap(panel.cycles(), fresh) == 0.0f, what + " isn't drawn as the oscillator plays it now");
        seen.push_back(panel.cycles());
    }
    int alike = 0;
    for (size_t a = 0; a < seen.size(); ++a)
        for (size_t b = a + 1; b < seen.size(); ++b) alike += largestGap(seen[a], seen[b]) > kApart ? 0 : 1;
    check(alike <= kAlikeOnOsc2, std::to_string(alike) + " pairs of waveforms draw alike");
    setParam(p, waveId, kSquare);
    panel.refresh();
    const std::vector<float> square = panel.cycles();
    setParam(p, pwId, kNarrow);
    panel.refresh();
    check(largestGap(square, panel.cycles()) > kApart, "the Pulse Width doesn't move a square's picture");
    setParam(p, pwId, 0.0f);
    setParam(p, waveId, 0.0f);
}

// A wave loaded into the OSC: the panel's view is the processor's newest, its columns hold the file's own extremes,
// and a second wave's view replaces the first's
void wavePicture(OscPanel& panel, VSynthEmuProcessor& p, int osc, const juce::File& folder, TestChecks& check) {
    setParam(p, Params::oscId(osc, Params::kOscType), kPcm);
    const juce::File file = writeTestWave(folder, kWaveSeconds);
    check(file.existsAsFile(), "the test wave couldn't be written");
    p.loadFile(file, osc);
    panel.refresh();
    const WaveDisplay::View* view = panel.view();
    check(view != nullptr && (int)view->columns.size() == WaveDisplay::View::kViewColumns,
          "a loaded wave has no view of its columns");
    if (view == nullptr || view->columns.empty()) return;
    float low = view->columns[0].low, high = view->columns[0].high;
    for (const auto& col : view->columns) {
        low = std::min(low, col.low);
        high = std::max(high, col.high);
    }
    const juce::Range<float> want = fileRange(file);
    check(low == want.getStart() && high == want.getEnd(),
          "the view spans " + std::to_string(low) + " to " + std::to_string(high) + ", the file " +
              std::to_string(want.getStart()) + " to " + std::to_string(want.getEnd()));
    p.loadFile(writeTestWave(folder, 1), osc);
    panel.refresh();
    check(panel.view() != view, "a second wave's view didn't replace the first's");
}

} // namespace

void waveChecks(SynthEditor& ed, VSynthEmuProcessor& p, const juce::File& folder, TestChecks& check) {
    const auto oscs = LayoutCheck::allOf<OscPanel>(ed.drawing());
    check(!oscs.empty(), "no OSC panel");
    if (oscs.empty()) return;
    const int osc = (int)oscs.size() - 1;   // example: the last OSC's panel; one constructor makes each (rackChecks)
    analogPictures(*oscs[(size_t)osc], p, osc, check);
    wavePicture(*oscs[(size_t)osc], p, osc, folder, check);
}
