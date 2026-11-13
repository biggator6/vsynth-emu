// D's OSC panel (UiOscPanel.h; UI step 2)
#include "UiOscPanel.h"
#include "PluginParamIds.h"
#include "UiLook.h"
#include "UiParts.h"
#include <array>

namespace {

using Controls::KnobRow;
constexpr int kFaceCount = OscPanel::kTypes, kPcm = 1;   // OSC Type's values: ANALOG 0, PCM 1
constexpr int kBigDial = 44, kSmallDial = 30, kBig = 4, kSmall = 6;
constexpr int kTitleH = 22, kDisplayH = 72, kRowGap = 7, kStatusH = 22;

// What each Type shows: the box beside Type, the big knobs and the small, as the processor's IDs for OSC 1
struct OscFace {
    const char* sub;
    std::array<KnobRow, kBig> big;
    std::array<KnobRow, kSmall> small;
};
constexpr OscFace kFaces[kFaceCount] = {
    { "anaWave",
      { { { "oscPitch", "Pitch" }, { "anaPw", "Pulse W" }, { "anaImpact", "Impact" }, { "oscLevel", "Level" } } },
      { { { "oscCoarse", "Coarse" }, { "oscFine", "Fine" }, { "anaPwKF", "PW KF" }, { "anaPwLfo", "PW LFO" },
          { "anaPwEnv", "PW Env" }, { "oscVelSens", "Vel Sens" } } } },
    { "encodeType",
      { { { "oscPitch", "Pitch" }, { "oscTime", "Time" }, { "oscFormant", "Formant" }, { "oscLevel", "Level" } } },
      { { { "oscCoarse", "Coarse" }, { "oscFine", "Fine" }, { "encodeDepth", "Depth" }, { "energy", "Energy" },
          { "oscVelSens", "Vel Sens" }, { "pitchKF", "Pitch KF" } } } },
};

// The analog waveforms' shapes, by Analog Waveform's value, as the display draws them
enum class Shape { saw, ramp, square, triangle, sine, noise };
constexpr Shape kShapes[] = { Shape::saw, Shape::square, Shape::triangle, Shape::sine, Shape::ramp, Shape::saw,
                              Shape::saw, Shape::square, Shape::noise, Shape::saw, Shape::square, Shape::saw,
                              Shape::sine, Shape::square };

double shapeAt(Shape s, double phase, int i) {
    switch (s) {
        case Shape::saw: return 1.0 - 2.0 * phase;
        case Shape::ramp: return 2.0 * phase - 1.0;
        case Shape::square: return phase < 0.5 ? 1.0 : -1.0;
        case Shape::triangle: return phase < 0.5 ? 4.0 * phase - 1.0 : 3.0 - 4.0 * phase;
        case Shape::sine: return std::sin(phase * juce::MathConstants<double>::twoPi);
        case Shape::noise: return juce::Random((juce::int64)i).nextDouble() * 2.0 - 1.0;
    }
    return 0.0;
}

// Knobs side by side across r, each its share of the width: n of them from `first` on
void placeRow(const std::vector<std::unique_ptr<Knob>>& knobs, size_t first, size_t n, juce::Rectangle<int> r) {
    const int w = r.getWidth() / (int)n;
    for (size_t i = first; i < juce::jmin(knobs.size(), first + n); ++i) knobs[i]->setBounds(r.removeFromLeft(w));
}

}

OscPanel::OscPanel(VSynthEmuProcessor& p, int osc)
    : FacedPanel(p.apvts, { "OSC " + juce::String(osc + 1), Params::oscId(osc, Params::kOscSwitch),
                            Params::oscId(osc, Params::kOscType) }, kFaceCount),
      processor_(p), osc_(osc) {
    Controls::box(playback_, p.apvts, id("playbackMode"), attached_);
    Controls::toggle(vari_, p.apvts, id("vari"), attached_);
    Controls::toggle(sync_, p.apvts, id(Params::kTempoSync), attached_);
    for (int face = 0; face < kFaceCount; ++face) {
        addSub(face, id(kFaces[face].sub));
        for (const KnobRow& k : kFaces[face].big) addKnob(face, id(k.id), k.name, kBigDial);
        for (const KnobRow& k : kFaces[face].small) addKnob(face, id(k.id), k.name, kSmallDial);
    }
    load_.onClick = [this] { chooseWave(); };
    for (juce::Component* c : std::initializer_list<juce::Component*> { &load_, &vari_, &playback_, &sync_ })
        addChildComponent(c);
    refresh();
}

juce::String OscPanel::id(const juce::String& name) const {
    return Params::oscId(osc_, name);
}

void OscPanel::faceShown() {
    for (juce::Component* c : std::initializer_list<juce::Component*> { &load_, &vari_, &playback_, &sync_ })
        c->setVisible(face() == kPcm);
}

void OscPanel::refresh() {
    FacedPanel::refresh();
    const juce::String wave = processor_.hasFileLoaded(osc_) ? processor_.getLoadedFileName(osc_) : "No wave loaded";
    if (wave == wave_) return;
    wave_ = wave;
    repaint(display_);
}

void OscPanel::resized() {
    auto r = getLocalBounds().reduced(Look::inset, 10);
    layoutTitle(r.removeFromTop(kTitleH), { 60, 76, 100 });
    r.removeFromTop(kRowGap);
    display_ = r.removeFromTop(kDisplayH);
    load_.setBounds(display_.reduced(6).removeFromTop(22).removeFromRight(60));   // on the wave, at its top right
    r.removeFromTop(kRowGap);
    const auto bigRow = r.removeFromTop(Knob::heightFor(kBigDial));
    r.removeFromTop(kRowGap);
    const auto smallRow = r.removeFromTop(Knob::heightFor(kSmallDial));
    for (const auto& face : knobs_) {
        placeRow(face, 0, kBig, bigRow);
        placeRow(face, kBig, kSmall, smallRow);
    }
    auto status = r.withTrimmedTop(r.getHeight() - kStatusH);
    vari_.setBounds(status.removeFromLeft(64));
    playback_.setBounds(status.removeFromLeft(110));
    status.removeFromLeft(10);
    sync_.setBounds(status.removeFromLeft(64));
}

// The wave's name for a PCM OSC (the wave itself is drawn in a later step), the waveform's shape for an analog one
void OscPanel::paintDisplay(juce::Graphics& g) const {
    Ui::fillRounded(g, display_.toFloat(), Look::field, 6.0f);
    if (face() == kPcm) {
        Ui::drawText(g, wave_, display_.reduced(10, 6).removeFromTop(16), { Look::sans(11.0f), Look::label });
        return;
    }
    const int wave = juce::roundToInt(processor_.apvts.getRawParameterValue(id(Params::kAnaWave))->load());
    const Shape shape = kShapes[(size_t)juce::jlimit(0, (int)std::size(kShapes) - 1, wave)];
    const auto area = display_.reduced(10, 14).toFloat();
    const int cycles = 3, steps = 240;
    juce::Path path;
    for (int i = 0; i <= steps; ++i) {
        const double t = (double)i / (double)steps, phase = std::fmod(t * (double)cycles, 1.0);
        const juce::Point<float> pt { area.getX() + (float)t * area.getWidth(),
                                      area.getCentreY() - 0.5f * area.getHeight() * (float)shapeAt(shape, phase, i) };
        i == 0 ? path.startNewSubPath(pt) : path.lineTo(pt);
    }
    Ui::stroke(g, path, Look::accent, juce::PathStrokeType(2.0f));
}

void OscPanel::chooseWave() {
    chooser_ = std::make_unique<juce::FileChooser>("Load a wave into OSC " + juce::String(osc_ + 1), juce::File(),
                                                   processor_.getFormatManager().getWildcardForAllFormats());
    chooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this](const juce::FileChooser& fc) {
                              const juce::File file = fc.getResult();
                              if (file.existsAsFile()) processor_.loadFile(file, osc_);
                              refresh();
                          });
}
