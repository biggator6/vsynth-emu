// The VC-2 mode's VOCODER page (UiVc2VocoderPage.h; v453)
#include "UiVc2VocoderPage.h"
#include "PluginVc2Params.h"
#include "UiFields.h"
#include "UiLook.h"
#include "UiPages.h"
#include "UiParts.h"
#include "UiVc2Mic.h"
#include "UiVc2Pages.h"
#include <cmath>

namespace {

constexpr int kMicW = 340, kVocoderH = 300, kDisplayW = 480, kLineH = 18, kNameW = 110, kStatusH = 64;
constexpr int kSmallCards = 4, kTypeW = 100, kWordsLines = 6;
constexpr int kType = 0x194, kSwitch = 0x193, kShift = 0x19f, kHireso = 1;   // [FW] Vocoder Type: LEGACY 0, HIRESO 1
// [FW] d-2, by the ports (Vc2Legacy::settings, Vc2Hireso::settings): what HIRESO alone reads (Env Attack and Release,
// Formant Template, Depth) and what LEGACY alone reads (Hold Dump Time)
constexpr int kHiresoOnly[] = { 0x19a, 0x19b, 0x1a0, 0x197 }, kLegacyOnly[] = { 0x19e };
constexpr float kFaint = 0.4f;
const char* const kWaits = "waits for the VC-2 part";
const char* const kStateNames[] = { "Input level", "Voiced", "Tracked pitch", "Auto Note", "Notes sounding",
                                    "Output level" };

// A line of text in a face and colour, a label so the layout's check sees it whole
std::unique_ptr<juce::Label> line(const juce::String& text, juce::Colour colour) {
    auto l = std::make_unique<juce::Label>();
    Look::setTextLook(*l, Look::sans(11.0f), colour, juce::Justification::centredLeft);
    l->setText(text, juce::dontSendNotification);
    return l;
}

// Text over several lines in a box (the status, the waiting displays), not a label: it wraps rather than cuts
class Words : public juce::Component {
public:
    explicit Words(juce::Colour c) : colour_(c) {}
    void say(const juce::String& t) {
        if (t == text_) return;
        text_ = t;
        repaint();
    }
    const juce::String& said() const { return text_; }
    void paint(juce::Graphics& g) override {
        Ui::drawWrapped(g, text_, getLocalBounds().reduced(6, 4), { Look::sans(11.0f), colour_ }, kWordsLines);
    }

private:
    juce::Colour colour_;
    juce::String text_;
};

// MIC IN: External In Type, the mic's state, the checks and the status line
class MicView : public juce::Component {
public:
    explicit MicView(VSynthEmuProcessor& p) : type_(p.apvts, vc2HostParamId(0x7a, 0, 0), "External In Type") {
        addAndMakeVisible(type_);
        for (const char* name : kStateNames) {
            names_.push_back(line(name, Look::label));
            values_.push_back(line(kWaits, Look::muted));
        }
        for (size_t i = 0; i < Vc2Mic::checks(p).size(); ++i) checks_.push_back(line({}, Look::text));
        for (auto* list : { &names_, &values_, &checks_ })
            for (auto& l : *list) addAndMakeVisible(*l);
        addAndMakeVisible(status_);
        update(p);
    }
    void update(VSynthEmuProcessor& p) {
        const auto checks = Vc2Mic::checks(p);
        for (size_t i = 0; i < checks.size(); ++i) {
            const auto& c = checks[i];
            const bool ok = c.state == Vc2Mic::State::ok, waits = c.state == Vc2Mic::State::waits;
            const juce::String mark = ok ? "OK" : waits ? "..." : "NO";
            Look::showText(*checks_[i], mark + "  " + c.name, ok ? Look::ready : waits ? Look::muted : Look::warning);
        }
        // v456: the part's state, as its view says, muted while no part has played
        const bool playing = p.vc2Play().view().playing.load(std::memory_order_relaxed);
        const auto state = Vc2Mic::stateLines(p);
        for (size_t i = 0; i < values_.size() && i < state.size(); ++i)
            Look::showText(*values_[i], state[i], playing ? Look::text : Look::muted);
        status_.say(Vc2Mic::status(p));
    }
    std::vector<juce::String> lines() const {
        std::vector<juce::String> out;
        for (const auto& c : checks_) out.push_back(c->getText());
        return out;
    }
    juce::String status() const { return status_.said(); }
    void resized() override {
        auto r = getLocalBounds();
        type_.setBounds(r.removeFromTop(Field::kHeight).withWidth(160));
        r.removeFromTop(ControlGrid::kGap);
        for (size_t i = 0; i < names_.size(); ++i) {
            auto row = r.removeFromTop(kLineH);
            names_[i]->setBounds(row.removeFromLeft(kNameW));
            values_[i]->setBounds(row);
        }
        r.removeFromTop(ControlGrid::kGap);
        for (auto& c : checks_) c->setBounds(r.removeFromTop(kLineH));
        r.removeFromTop(ControlGrid::kGap);
        status_.setBounds(r.removeFromTop(kStatusH));
    }

private:
    Field type_;
    std::vector<std::unique_ptr<juce::Label>> names_, values_, checks_;
    Words status_ { Look::label };
};

// FORMANT's warp: each side's ratio about its pivot, drawn as Formant Shift bends the line (a drawing of its settings)
class WarpView : public juce::Component {
public:
    explicit WarpView(Controls::State& s) : watch_(s, { vc2HostParamId(kShift, 0, 0) }, [this] { repaint(); }) {}
    void paint(juce::Graphics& g) override {
        const auto r = getLocalBounds().toFloat();
        Ui::fillRounded(g, r, Look::field, 6.0f);
        const auto a = r.reduced(8.0f);
        const float bend = std::exp2((watch_.proportion(0) - 0.5f) * 2.0f);
        std::vector<juce::Point<float>> points;
        for (int i = 0; i <= 48; ++i) {
            const float x = (float)i / 48.0f;
            points.push_back({ a.getX() + x * a.getWidth(), a.getBottom() - std::pow(x, 1.0f / bend) * a.getHeight() });
        }
        Ui::stroke(g, Ui::polyline({ a.getBottomLeft(), a.getTopRight() }), Look::divider, juce::PathStrokeType(1.0f));
        Ui::stroke(g, Ui::polyline(points), Look::accent, juce::PathStrokeType(1.6f));
    }

private:
    Watch watch_;
};

// The VOCODER card's body: the engine's bands' display (d-14), which waits for the VC-2 synth, beside its controls
class VocoderBody : public juce::Component {
public:
    VocoderBody(Controls::State& s, const ControlGrid::Rows& rows) : grid_(s, rows) {
        bands_.say("The engine's bands as they play, the mic's envelope in each and the output's (LEGACY's 10, "
                   "HIRESO's 32), wait for the VC-2 synth's view of its state (step 4.3)");
        addAndMakeVisible(bands_);
        addAndMakeVisible(grid_);
    }
    ControlGrid& grid() { return grid_; }
    void paint(juce::Graphics& g) override { Ui::fillRounded(g, bands_.getBounds().toFloat(), Look::field, 6.0f); }
    void resized() override {
        auto r = getLocalBounds();
        bands_.setBounds(r.removeFromLeft(kDisplayW));
        r.removeFromLeft(2 * ControlGrid::kGap);
        grid_.setBounds(r);
    }

private:
    Words bands_ { Look::muted };
    ControlGrid grid_;
};

ControlGrid::Rows rowsOf(std::initializer_list<Controls::KnobRow> rows) {
    ControlGrid::Rows made;
    for (const Controls::KnobRow& r : rows) made.push_back({ Vc2Pages::patchId(r.id), r.name });
    return made;
}

}

GridAndDrawing::GridAndDrawing(Controls::State& state, const ControlGrid::Rows& rows,
                               std::unique_ptr<juce::Component> drawing)
    : grid_(state, rows), drawing_(std::move(drawing)) {
    addAndMakeVisible(grid_);
    addAndMakeVisible(*drawing_);
}

void GridAndDrawing::resized() {
    auto r = getLocalBounds();
    grid_.setBounds(r.removeFromTop(grid_.heightFor(r.getWidth())));
    r.removeFromTop(ControlGrid::kGap);
    drawing_->setBounds(r.removeFromTop(kDrawingH));
}

Vc2VocoderPage::Vc2VocoderPage(VSynthEmuProcessor& p)
    : processor_(p), type_(p.apvts, { vc2HostParamId(kType, 0, 0) }, [this] { dimByEngine(); }) {
    auto& s = p.apvts;
    mic_ = std::make_unique<Card>("MIC IN");
    mic_->setBody(std::make_unique<MicView>(p));
    vocoder_ = std::make_unique<Card>("VOCODER");
    auto on = std::make_unique<Pages::TitleSwitch>(s, vc2HostParamId(kSwitch, 0, 0));
    vocoder_->toTitle(std::move(on), Pages::TitleSwitch::kWidth);
    vocoder_->toTitle(std::make_unique<Pages::TitleBox>(s, vc2HostParamId(kType, 0, 0)), kTypeW);
    auto& body = vocoder_->setBody(std::make_unique<VocoderBody>(
        s, rowsOf({ { "196", "Sound" }, { "197", "Depth" }, { "195", "Direct" }, { "1a3", "Robot" },
                    { "19a", "Attack" }, { "19b", "Release" }, { "1a4", "Stereo" } })));
    grids_.push_back(&body.grid());
    formant_ = std::make_unique<Card>("FORMANT");
    auto& warp = formant_->setBody(std::make_unique<GridAndDrawing>(
        s, rowsOf({ { "19f", "Shift" }, { "1a0", "Template" } }), std::make_unique<WarpView>(s)));
    grids_.push_back(&warp.grid());
    consonants_ =
        Pages::gridCard(s, "CONSONANTS", rowsOf({ { "198", "Detect" }, { "199", "Level" }, { "1a5", "Spread" } }));
    keys_ = Pages::gridCard(s, "KEYS & HOLD", rowsOf({ { "1a2", "Key Mode" }, { "1a1", "Split" }, { "19e", "Dump" } }));
    autoNote_ = Pages::gridCard(s, "AUTO NOTE", rowsOf({ { "19c", "Auto Note" }, { "19d", "Retrigger" } }));
    for (Card* c : { consonants_.get(), keys_.get(), autoNote_.get() })
        grids_.push_back(dynamic_cast<ControlGrid*>(c->body()));
    for (Card* c : { mic_.get(), vocoder_.get(), formant_.get(), consonants_.get(), keys_.get(), autoNote_.get() })
        addAndMakeVisible(c);
    Look::setTextLook(engineWord_, Look::sans(11.0f), Look::muted, juce::Justification::centredLeft);
    addAndMakeVisible(engineWord_);
    dimByEngine();
}

Vc2VocoderPage::~Vc2VocoderPage() = default;

// d-2: what the chosen engine doesn't read, faint, and which engine reads it said
void Vc2VocoderPage::dimByEngine() {
    if (grids_.empty()) return;   // the watch's first call, before the cards are made
    const bool hireso = juce::roundToInt(type_.value(0)) == kHireso;
    const auto dim = [this](const int* ids, size_t n, bool faint) {
        for (size_t i = 0; i < n; ++i)
            for (ControlGrid* g : grids_)
                if (juce::Component* c = g->control(vc2HostParamId(ids[i], 0, 0))) c->setAlpha(faint ? kFaint : 1.0f);
    };
    dim(kHiresoOnly, std::size(kHiresoOnly), !hireso);
    dim(kLegacyOnly, std::size(kLegacyOnly), hireso);
    engineWord_.setText(hireso ? "Faint: LEGACY alone reads it (Dump)"
                               : "Faint: HIRESO alone reads them (Depth, Attack, Release, Template)",
                        juce::dontSendNotification);
}

void Vc2VocoderPage::refresh() {
    if (auto* mic = dynamic_cast<MicView*>(mic_->body())) mic->update(processor_);
}

juce::String Vc2VocoderPage::status() const {
    auto* mic = dynamic_cast<MicView*>(mic_->body());
    return mic != nullptr ? mic->status() : juce::String();
}

std::vector<juce::String> Vc2VocoderPage::checkLines() const {
    auto* mic = dynamic_cast<MicView*>(mic_->body());
    return mic != nullptr ? mic->lines() : std::vector<juce::String>();
}

// MIC IN down the left; VOCODER across the top right, the line naming the faint ones' engine under it, then the four
// small cards across
void Vc2VocoderPage::resized() {
    auto r = getLocalBounds().reduced(Look::inset);
    mic_->setBounds(r.removeFromLeft(kMicW));
    r.removeFromLeft(Look::gap);
    vocoder_->setBounds(r.removeFromTop(kVocoderH));
    engineWord_.setBounds(r.removeFromTop(kLineH));
    r.removeFromTop(Look::gap);
    const int w = (r.getWidth() - (kSmallCards - 1) * Look::gap) / kSmallCards;
    for (Card* c : { formant_.get(), consonants_.get(), keys_.get(), autoNote_.get() }) {
        auto* grid = dynamic_cast<ControlGrid*>(c->body());
        auto* drawn = dynamic_cast<GridAndDrawing*>(c->body());
        const int body = grid ? grid->heightFor(c->bodyWidthFor(w)) : drawn ? drawn->heightFor(c->bodyWidthFor(w)) : 0;
        c->setBounds(r.removeFromLeft(w).withHeight(c->heightFor(body)));
        r.removeFromLeft(Look::gap);
    }
}
