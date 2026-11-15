// D's OSC panel (UiOscPanel.h; UI step 2)
#include "UiOscPanel.h"
#include "PluginParamIds.h"
#include "UiLook.h"
#include "UiParts.h"
#include <array>

namespace {

using Controls::KnobRow;
constexpr int kFaceCount = OscPanel::kTypes, kPcm = 1, kExtIn = 2;   // OSC Type's values: ANALOG 0, PCM 1, EXT IN 2
constexpr int kBigDial = 44, kSmallDial = 30, kBig = 4, kSmall = 6;
constexpr int kTitleH = 22, kDisplayH = 72, kRowGap = 7, kStatusH = 22, kNameH = 16;
constexpr float kPercent = 100.0f;   // Loop Start and End are percents of the wave

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
    // EXT IN (v206): what its converter reads (HARDWARE_FIRMWARE.md, "The EXT IN OSC"), no box beside Type
    { "",
      { { { "oscPitch", "Pitch" }, { "oscCoarse", "Coarse" }, { "oscFine", "Fine" }, { "oscLevel", "Level" } } },
      { { { "waveGain", "Gain" }, { "pitchKF", "Pitch KF" }, { "pitchEnvDepth", "Env Depth" },
          { "randomPitch", "Random" }, { "oscLevelKF", "Level KF" }, { "oscVelSens", "Vel Sens" } } } },
};

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
    const bool input = processor_.hasInput();
    if (wave != wave_ || input != input_) {
        wave_ = wave;
        input_ = input;
        repaint(display_);
    }
    if (face() == kPcm) refreshWave();
    else if (face() != kExtIn) refreshCycles();   // EXT IN draws its words, no cycles
}

// A PCM OSC's picture: the processor's newest view of its wave and the loop's span, repainted only when either moved
void OscPanel::refreshWave() {
    const auto value = [this](const juce::String& name) {
        return processor_.apvts.getRawParameterValue(id(name))->load();
    };
    const WaveDisplay::Loop loop { value(Params::kLoop) >= 0.5f, value(Params::kLoopStart) / kPercent,
                                   value(Params::kLoopEnd) / kPercent };
    const int gen = processor_.waveViewGen(osc_);
    if (gen == viewGen_ && loop == loop_) return;
    viewGen_ = gen;
    view_ = processor_.waveView(osc_);
    loop_ = loop;
    repaint(display_);
}

// An analog OSC's picture: the oscillator's own cycles, made again only when a setting it reads has moved
void OscPanel::refreshCycles() {
    const WaveDisplay::AnalogSettings now = WaveDisplay::analogSettings(processor_.currentControls(), osc_);
    if (!cycles_.empty() && now == drawn_) return;
    drawn_ = now;
    cycles_ = WaveDisplay::analogCycles(now);
    repaint(display_);
}

juce::String OscPanel::noInputWord() const {
    return processor_.hasInput() ? juce::String() : "No input: the host gives none, or the input is off";
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

// A PCM OSC: its wave, the loop's span shaded, its name over it; an analog OSC: the oscillator's own cycles (v200);
// EXT IN: the outside input by External In Type (v206), and why it's silent if there's none (v207)
void OscPanel::paintDisplay(juce::Graphics& g) const {
    Ui::fillRounded(g, display_.toFloat(), Look::field, 6.0f);
    if (face() == kExtIn) {
        const auto* type = processor_.apvts.getParameter("fxExtInType");
        const juce::String text = "External input, " + (type ? type->getCurrentValueAsText() : juce::String());
        auto lines = display_.reduced(10, 6);
        Ui::drawText(g, text, lines.removeFromTop(16), { Look::sans(11.0f), Look::label });
        Ui::drawText(g, noInputWord(), lines.removeFromTop(16), { Look::sans(11.0f), Look::warning });
        return;
    }
    if (face() != kPcm) {
        WaveDisplay::paintCycles(g, display_.reduced(10, 14).toFloat(), cycles_);
        return;
    }
    auto inner = display_.reduced(10, 6);
    const auto name = inner.removeFromTop(kNameH);
    if (view_ && processor_.hasFileLoaded(osc_)) WaveDisplay::paintWave(g, inner.toFloat(), *view_, loop_);
    Ui::drawText(g, wave_, name, { Look::sans(11.0f), Look::label });
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
