// The Time Trip pad (UiTripPad.h; v202)
#include "UiTripPad.h"
#include "UiLook.h"
#include "UiParts.h"

namespace {
constexpr int kTripX = 10, kTripY = 11;   // Time Trip Assign X and Y, in VSynthEmuProcessor::assignParamId's order
constexpr int kOff = 0, kAlsoOff = 32;    // an assign's values that name no controller (0x894d82e0)
constexpr int kLowest = 1, kSpan = 126;   // a finger's values, 1…127; 0 is none
constexpr int kChannel = 1, kGrid = 3;
constexpr float kRing = 0.4f;             // the circle to scrub round, as a share of the pad's shorter side
}

int TripPad::assigned(int assign) const {
    if (assigns_) return assign == kTripX ? assigns_().x : assigns_().y;
    const auto* id = VSynthEmuProcessor::assignParamId(assign);
    const int cc = juce::roundToInt(processor_.apvts.getRawParameterValue(id)->load());
    return cc == kOff || cc == kAlsoOff ? 0 : cc;
}

bool TripPad::canSend() const {
    return assigned(kTripX) != 0 && assigned(kTripY) != 0;
}

juce::Point<int> TripPad::valueAt(juce::Point<float> at) const {
    const auto r = getLocalBounds().toFloat();
    const auto value = [](float share) { return kLowest + juce::jlimit(0, kSpan, juce::roundToInt(share * kSpan)); };
    return { value((at.x - r.getX()) / r.getWidth()), value((r.getBottom() - at.y) / r.getHeight()) };
}

void TripPad::send(juce::Point<int> value) {
    if (!canSend()) return;
    processor_.sendFromEditor(juce::MidiMessage::controllerEvent(kChannel, assigned(kTripX), value.x));
    processor_.sendFromEditor(juce::MidiMessage::controllerEvent(kChannel, assigned(kTripY), value.y));
}

void TripPad::lift() {
    if (!finger_) return;
    send({ 0, 0 });
    finger_.reset();
    repaint();
}

void TripPad::mouseDown(const juce::MouseEvent& e) {
    down_ = true;
    finger_ = valueAt(e.position);
    send(*finger_);
    repaint();
}

void TripPad::mouseDrag(const juce::MouseEvent& e) {
    const juce::Point<int> now = valueAt(e.position);
    if (finger_ == now) return;
    finger_ = now;
    send(now);
    repaint();
}

void TripPad::mouseUp(const juce::MouseEvent&) {
    down_ = false;
    if (!hold_) lift();
}

void TripPad::setHold(bool on) {
    hold_ = on;
    if (!on && !down_) lift();
}

// The field, its thirds and the circle to scrub round; the finger and a line to it from the centre, with its values;
// with no finger a hint, and with an assign OFF why the pad does nothing
void TripPad::paint(juce::Graphics& g) {
    const auto r = getLocalBounds().toFloat();
    Ui::fillRounded(g, r, Look::field, 6.0f);
    g.setColour(Look::divider);
    for (int i = 1; i < kGrid; ++i) {
        const float x = r.getX() + r.getWidth() * (float)i / (float)kGrid;
        const float y = r.getY() + r.getHeight() * (float)i / (float)kGrid;
        g.drawLine(x, r.getY(), x, r.getBottom());
        g.drawLine(r.getX(), y, r.getRight(), y);
    }
    const float ring = kRing * std::min(r.getWidth(), r.getHeight());
    g.drawEllipse(juce::Rectangle<float>(2.0f * ring, 2.0f * ring).withCentre(r.getCentre()), 1.0f);
    auto text = getLocalBounds().reduced(8, 6);
    if (!canSend() || !finger_) {
        const bool off = !canSend();
        Ui::drawText(g, off ? "Trip Assign X or Y is OFF" : "Press, then circle", text.removeFromBottom(14),
                     { Look::sans(10.0f), off ? Look::warning : Look::muted, juce::Justification::centred });
        return;
    }
    const juce::Point<float> at { r.getX() + r.getWidth() * (float)(finger_->x - kLowest) / (float)kSpan,
                                  r.getBottom() - r.getHeight() * (float)(finger_->y - kLowest) / (float)kSpan };
    Ui::stroke(g, Ui::polyline({ r.getCentre(), at }), Look::mod.withAlpha(0.5f), juce::PathStrokeType(1.5f));
    Ui::dot(g, at, 14.0f, Look::mod);
    Ui::drawText(g, juce::String(finger_->x) + ", " + juce::String(finger_->y), text.removeFromTop(14),
                 { Look::mono(10.0f), Look::muted });
}
