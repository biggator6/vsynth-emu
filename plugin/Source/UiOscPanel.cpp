// D's OSC panel (UiOscPanel.h; UI step 2; a mode's faces, v453)
#include "UiOscPanel.h"
#include "PluginParamIds.h"
#include "UiLook.h"
#include "UiParts.h"
#include <array>

namespace {

using Controls::KnobRow;
constexpr int kBigDial = 44, kSmallDial = 30;
constexpr int kTitleH = 22, kDisplayH = 72, kRowGap = 7, kStatusH = 22, kNameH = 16;
constexpr float kPercent = 100.0f;   // Loop Start and End are percents of the wave

// What each of the V-Synth's Types shows (ANALOG 0, PCM 1, EXT IN 2): the box beside Type, the big knobs and the small,
// as the processor's IDs for OSC 1
constexpr OscPanel::Face kFaces[OscPanel::kTypes] = {
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

juce::String vsynthId(int osc, const juce::String& name) {
    return Params::oscId(osc, name);
}

// The V-Synth's OSC panel: its three faces, its PCM line, a PCM OSC loading its wave, the analog cycles from its controls
OscPanel::Model vsynthModel(VSynthEmuProcessor& p) {
    OscPanel::Model m;
    m.faces = kFaces;
    m.count = OscPanel::kTypes;
    m.id = vsynthId;
    m.switchRow = Params::kOscSwitch;
    m.typeRow = Params::kOscType;
    m.line = { "vari", "playbackMode", Params::kTempoSync };
    m.extInType = "fxExtInType";
    m.analog = [&p](int osc) { return WaveDisplay::analogSettings(p.currentControls(), osc); };
    return m;
}

// Knobs side by side across r, each its share of the width: n of them from `first` on
void placeRow(const std::vector<std::unique_ptr<Knob>>& knobs, size_t first, size_t n, juce::Rectangle<int> r) {
    const int w = r.getWidth() / (int)n;
    for (size_t i = first; i < juce::jmin(knobs.size(), first + n); ++i) knobs[i]->setBounds(r.removeFromLeft(w));
}

}

OscPanel::OscPanel(VSynthEmuProcessor& p, int osc) : OscPanel(p, osc, vsynthModel(p)) {}

OscPanel::OscPanel(VSynthEmuProcessor& p, int osc, Model model)
    : FacedPanel(p.apvts, { "OSC " + juce::String(osc + 1), model.id(osc, model.switchRow),
                            model.id(osc, model.typeRow) }, model.count),
      processor_(p), osc_(osc), model_(std::move(model)) {
    Controls::box(playback_, p.apvts, id(model_.line[1]), attached_);
    Controls::toggle(vari_, p.apvts, id(model_.line[0]), attached_);
    Controls::toggle(sync_, p.apvts, id(model_.line[2]), attached_);
    for (int face = 0; face < model_.count; ++face) {
        addSub(face, id(model_.faces[face].sub));
        for (const KnobRow& k : model_.faces[face].big) addKnob(face, id(k.id), k.name, kBigDial);
        for (const KnobRow& k : model_.faces[face].small) addKnob(face, id(k.id), k.name, kSmallDial);
    }
    load_.onClick = [this] { chooseWave(); };
    for (juce::Component* c : pcmLine()) addChildComponent(c);
    refresh();
}

juce::String OscPanel::id(const juce::String& name) const {
    return model_.id(osc_, name);
}

// What a PCM OSC shows besides its knobs: Load... where it loads a file, and the line of Vari, Playback Mode and Sync
std::vector<juce::Component*> OscPanel::pcmLine() {
    std::vector<juce::Component*> line { &vari_, &playback_, &sync_ };
    if (model_.loads) line.push_back(&load_);
    return line;
}

void OscPanel::faceShown() {
    for (juce::Component* c : pcmLine())
        c->setVisible(face() == model_.pcm);
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
    if (face() == model_.pcm && model_.loads) refreshWave();
    else if (face() != model_.extIn && face() != model_.pcm && face() != model_.vocoder) refreshCycles();
    if (!model_.draw) return;
    juce::String now;   // a mode's own drawing: its face's knobs as they stand
    now << face();
    for (const auto& k : knobs_[(size_t)juce::jmax(0, face())]) now << '|' << k->dial().getValue();
    if (now == shown_) return;
    shown_ = now;
    repaint(display_);
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
    const WaveDisplay::AnalogSettings now = model_.analog(osc_);
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

// EXT IN: the outside input by External In Type (v206), and why it's silent if there's none (v207)
void OscPanel::paintExtIn(juce::Graphics& g) const {
    const juce::String type = Controls::currentText(processor_.apvts, model_.extInType);
    auto lines = display_.reduced(10, 6);
    Ui::drawText(g, "External input, " + type, lines.removeFromTop(16), { Look::sans(11.0f), Look::label });
    Ui::drawText(g, noInputWord(), lines.removeFromTop(16), { Look::sans(11.0f), Look::warning });
    if (model_.draw) model_.draw(g, lines, osc_, face());
}

// A PCM OSC: its wave, the loop's span shaded, its name over it; an analog OSC: the oscillator's own cycles (v200);
// EXT IN its words; what a mode draws itself (the VC-2's PCM and VOCODER, v453)
void OscPanel::paintDisplay(juce::Graphics& g) const {
    Ui::fillRounded(g, display_.toFloat(), Look::field, 6.0f);
    if (face() == model_.extIn) return paintExtIn(g);
    if (model_.draw && (face() == model_.vocoder || (face() == model_.pcm && !model_.loads)))
        return model_.draw(g, display_.reduced(10, 6), osc_, face());
    if (face() != model_.pcm) {
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
