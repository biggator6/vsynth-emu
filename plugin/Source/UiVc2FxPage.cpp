// The VC-2 mode's FX page (UiVc2FxPage.h; v453)
#include "UiVc2FxPage.h"
#include "UiControls.h"
#include "UiLook.h"
#include "UiPages.h"
#include "UiParts.h"
#include "UiVc2Pages.h"
#include "UiVc2VocoderPage.h"
#include "UiWatch.h"
#include <cmath>

namespace {

constexpr int kLeftW = 576, kWaitH = 48, kWaitLines = 3;
constexpr float kLowHz = 20.0f, kHighHz = 20000.0f, kRangeDb = 15.0f;
const char* const kWaits = "The types' own parameters, edits by ID as the V-Synth's, and the types' names wait for "
                           "rom.4's effect driver in the plugin, with the VC-2 synth's effects (step 4.3's wiring)";

// The effects' cards (UI_DESIGN_VC2.md, "FX"): each its switch on its title, its Type and levels
const Pages::CardRows kEffects[] = {
    { "MFX", { { { "1b7", "Type" }, { "1b8", "Master" }, { "1b9", "To Chorus" }, { "1ba", "To Reverb" } } }, 0, "13d" },
    { "CHORUS", { { { "23c", "Type" }, { "23d", "Master" }, { "23e", "To Reverb" } } }, 0, "13e" },
    { "REVERB", { { { "270", "Type" }, { "271", "Master" } } }, 0, "13f" },
    { "SYSTEM", { { { "40", "Master" }, { "78", "Output Gain" } } }, 1 },
};
// [FW] the patch EQ's bands: Freq, Q and Gain each
const char* const kEq[] = { "1af", "1b0", "1b1", "1b2", "1b3", "1b4" };
constexpr size_t kBands = 2, kPerBand = 3;

// The PATCH EQ's two bands as a curve across 20 Hz…20 kHz: each a bell at its frequency, as wide as its Q gives, as high as
// its gain, by their values in the editor's text (a drawing of the settings)
class EqCurve : public juce::Component {
public:
    explicit EqCurve(Controls::State& s) : state_(s), watch_(s, ids(), [this] { repaint(); }) {}
    void paint(juce::Graphics& g) override {
        const auto r = getLocalBounds().toFloat();
        Ui::fillRounded(g, r, Look::field, 6.0f);
        const auto a = r.reduced(8.0f);
        g.setColour(Look::divider);
        g.drawHorizontalLine(juce::roundToInt(a.getCentreY()), a.getX(), a.getRight());
        std::vector<juce::Point<float>> points;
        for (int i = 0; i <= 96; ++i) {
            const float x = (float)i / 96.0f;
            const float octaves = std::log2(kHighHz / kLowHz) * x;
            float db = 0.0f;
            for (size_t b = 0; b < kBands; ++b) db += bell(b, octaves);
            points.push_back({ a.getX() + x * a.getWidth(), a.getCentreY() - db / kRangeDb * 0.5f * a.getHeight() });
        }
        Ui::stroke(g, Ui::polyline(points), Look::accent, juce::PathStrokeType(1.6f));
    }

private:
    Controls::State& state_;
    Watch watch_;
    static juce::StringArray ids() {
        juce::StringArray out;
        for (const char* row : kEq) out.add(Vc2Pages::patchId(row));
        return out;
    }
    float number(size_t i) const {
        const juce::String id = Vc2Pages::patchId(kEq[i]);
        return (float)Controls::currentText(state_, id).getDoubleValue();
    }
    // A band's gain at `octaves` above 20 Hz
    float bell(size_t band, float octaves) const {
        const float hz = juce::jmax(kLowHz, number(band * kPerBand));
        const float q = juce::jmax(0.1f, number(band * kPerBand + 1));
        const float away = (octaves - std::log2(hz / kLowHz)) * q;
        return number(band * kPerBand + 2) * std::exp(-0.5f * away * away);
    }
};

// A card's line of what waits, and why
class WaitLine : public juce::Component {
public:
    void paint(juce::Graphics& g) override {
        Ui::drawWrapped(g, kWaits, getLocalBounds(), { Look::sans(11.0f), Look::muted }, kWaitLines);
    }
};

}

Vc2FxPage::Vc2FxPage(VSynthEmuProcessor& p) {
    auto& s = p.apvts;
    for (const Pages::CardRows& rows : kEffects)
        (rows.column == 0 ? left_ : right_).push_back(Pages::gridCard(s, rows, Vc2Pages::patchId));
    ControlGrid::Rows eq;
    const char* const names[] = { "EQ 1 Freq", "EQ 1 Q", "EQ 1 Gain", "EQ 2 Freq", "EQ 2 Q", "EQ 2 Gain" };
    for (size_t i = 0; i < std::size(kEq); ++i) eq.push_back({ Vc2Pages::patchId(kEq[i]), names[i] });
    auto patchEq = std::make_unique<Card>("PATCH EQ");
    patchEq->setBody(std::make_unique<GridAndDrawing>(s, eq, std::make_unique<EqCurve>(s)));
    right_.push_back(std::move(patchEq));
    auto types = std::make_unique<Card>("THE TYPES' PARAMETERS");
    types->setBody(std::make_unique<WaitLine>());
    left_.push_back(std::move(types));
    for (auto* list : { &left_, &right_ })
        for (auto& c : *list) addAndMakeVisible(*c);
}

// The effects and what waits down the left, the System and the PATCH EQ down the right
void Vc2FxPage::resized() {
    auto r = getLocalBounds().reduced(Look::inset);
    const auto left = r.removeFromLeft(kLeftW);
    r.removeFromLeft(Look::gap);
    std::vector<Card*> down;
    for (auto& c : left_) down.push_back(c.get());
    Pages::stack(down, left, false);
    down.back()->setSize(down.back()->getWidth(), down.back()->heightFor(kWaitH));
    auto right = r;
    for (auto& c : right_) {
        auto* drawn = dynamic_cast<GridAndDrawing*>(c->body());
        auto* grid = dynamic_cast<ControlGrid*>(c->body());
        const int w = c->bodyWidthFor(right.getWidth());
        const int body = drawn ? drawn->heightFor(w) : grid ? grid->heightFor(w) : 0;
        c->setBounds(right.removeFromTop(c->heightFor(body)));
        right.removeFromTop(Look::gap);
    }
}
