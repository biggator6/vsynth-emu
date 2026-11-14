// D's look: the faces, and the LookAndFeel's drawing of knobs, switches, boxes and buttons (UI step 1; UiLook.h)
#include "UiLook.h"
#include "UiParts.h"
#include <algorithm>
#include <utility>

namespace Look {

juce::Font sans(float size, const char* style, float spacing) {
    return juce::Font(juce::FontOptions().withPointHeight(size).withStyle(style).withKerningFactor(spacing));
}

juce::Font mono(float size) {
    return juce::Font(juce::FontOptions().withName(juce::Font::getDefaultMonospacedFontName()).withPointHeight(size));
}

void setTextLook(juce::Label& l, const juce::Font& font, juce::Colour colour, juce::Justification just) {
    l.setFont(font);
    l.setColour(juce::Label::textColourId, colour);
    l.setJustificationType(just);
    l.setBorderSize({ 0, 0, 0, 0 });
    l.setMinimumHorizontalScale(0.8f);
    l.setInterceptsMouseClicks(false, false);
}

void paintPanel(juce::Graphics& g, juce::Rectangle<int> bounds, bool selected) {
    const auto r = bounds.toFloat().reduced(0.5f);
    Ui::fillRounded(g, r, panel, (float)radius);
    g.setColour(selected ? accent : edge);
    g.drawRoundedRectangle(r, (float)radius, 1.0f);
}

void panelName(juce::Label& l, const juce::String& name) {
    setTextLook(l, sans(12.0f, "Bold", 0.06f), text, juce::Justification::centredLeft);
    l.setText(name, juce::dontSendNotification);
}

void fieldName(juce::Label& l, const juce::String& name) {
    setTextLook(l, sans(10.0f, "Semibold"), label, juce::Justification::centredLeft);
    l.setText(name, juce::dontSendNotification);
}

void showText(juce::Label& l, const juce::String& words, juce::Colour colour) {
    l.setText(words, juce::dontSendNotification);
    l.setColour(juce::Label::textColourId, colour);
}

namespace {

// A knob's circle: its centre, radius and the width its arcs are drawn
struct Circle {
    juce::Point<float> c;
    float r, stroke;
};

// An arc of a circle from one angle to another, as JUCE counts angles, with round ends
void arc(juce::Graphics& g, const Circle& k, float a0, float a1, juce::Colour colour) {
    if (a1 - a0 < 0.005f) return;
    juce::Path p;
    p.addCentredArc(k.c.x, k.c.y, k.r, k.r, 0.0f, a0, a1, true);
    const juce::PathStrokeType round(k.stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    Ui::stroke(g, p, colour, round);
}

// Where the value arc starts: 0 on a range that holds both signs, else the range's start
float zeroOf(juce::Slider& s) {
    return s.getMinimum() < 0.0 && s.getMaximum() > 0.0 ? (float)s.valueToProportionOfLength(0.0) : 0.0f;
}

juce::Point<float> onCircle(juce::Point<float> c, float r, float angle) {
    return { c.x + r * std::sin(angle), c.y - r * std::cos(angle) };
}

// The modulation's reach, a ring outside the track, if the knob's owner has set one
void modRing(juce::Graphics& g, juce::Slider& s, const Circle& k, float start, float end) {
    const auto& props = s.getProperties();
    if (!props.contains(modLo) || !props.contains(modHi)) return;
    const float lo = juce::jlimit(0.0f, 1.0f, (float)props[modLo]), hi = juce::jlimit(0.0f, 1.0f, (float)props[modHi]);
    arc(g, { k.c, k.r + 3.2f, 2.2f }, start + lo * (end - start), start + hi * (end - start), mod);
}

// The colours the LookAndFeel gives every control it draws
const std::pair<int, juce::Colour> kColours[] = {
    { juce::Label::textColourId, text },
    { juce::ComboBox::textColourId, text },
    { juce::ComboBox::arrowColourId, label },
    { juce::PopupMenu::backgroundColourId, panel },
    { juce::PopupMenu::textColourId, text },
    { juce::PopupMenu::highlightedBackgroundColourId, accent },
    { juce::PopupMenu::highlightedTextColourId, ground },
    { juce::TextButton::textColourOffId, text },
    { juce::TextButton::textColourOnId, ground },
    { juce::ToggleButton::textColourId, text },
    { juce::Slider::textBoxTextColourId, text },
};

}

Feel::Feel() {
    for (const auto& [id, colour] : kColours) setColour(id, colour);
}

// The knob (UI_DESIGN.md, "The look"): the sweep's track, the value arc from the bottom or, on a range of both signs,
// from the top, the modulation's ring, a cap on a big knob, and the pointer
// one-off: JUCE's LookAndFeel override; its parameters are JUCE's
void Feel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end,
                            juce::Slider& s) {
    const float side = (float)juce::jmin(w, h), big = 40.0f;
    const Circle k { { (float)x + (float)w * 0.5f, (float)y + (float)h * 0.5f }, side * 0.5f - 4.0f,
                     side >= big ? 4.0f : 3.0f };
    const float at = start + pos * (end - start), from = start + zeroOf(s) * (end - start);
    arc(g, k, start, end, track);
    arc(g, k, juce::jmin(from, at), juce::jmax(from, at), s.isEnabled() ? accent : dim);
    modRing(g, s, k, start, end);
    if (side >= big) {
        Ui::dot(g, k.c, 2.0f * (k.r - 7.0f), divider);
    }
    g.setColour(text);
    g.drawLine(juce::Line<float>(k.c, onCircle(k.c, k.r - 6.0f, at)), side >= big ? 2.0f : 1.6f);
}

namespace {

// What a linear slider fills: from its range's zero to its value, across from the left or up from the bottom
juce::Rectangle<float> fillAcross(juce::Rectangle<float> r, float pos, juce::Slider& s) {
    const float from = r.getX() + zeroOf(s) * r.getWidth();
    return r.withLeft(juce::jmin(from, pos)).withRight(juce::jmax(from, pos));
}
juce::Rectangle<float> fillUp(juce::Rectangle<float> r, float pos, juce::Slider& s) {
    const float from = r.getBottom() - zeroOf(s) * r.getHeight();
    return r.withTop(juce::jmin(from, pos)).withBottom(juce::jmax(from, pos));
}

juce::Colour fillColour(juce::Slider& s) {
    return s.findColour(juce::Slider::trackColourId).withAlpha(s.isEnabled() ? 0.6f : 0.25f);
}

// A bar (UiFields.h): a field, its fill in the bar's track colour, its value's text on it
void bar(juce::Graphics& g, juce::Rectangle<float> r, juce::Rectangle<float> fill, juce::Slider& s) {
    Ui::fillRounded(g, r, field, 3.0f);
    Ui::fillRounded(g, fill, fillColour(s), 3.0f);
    Ui::drawText(g, s.getTextFromValue(s.getValue()), r.toNearestInt(),
                 { mono(10.0f), text, juce::Justification::centred, false });
}
void barAcross(juce::Graphics& g, juce::Rectangle<float> r, float pos, juce::Slider& s) {
    bar(g, r, fillAcross(r, pos, s), s);
}
void barUp(juce::Graphics& g, juce::Rectangle<float> r, float pos, juce::Slider& s) {
    bar(g, r, fillUp(r, pos, s), s);
}
// A plain slider across (the Effects panel's): a thin track, its fill, a dot at the value
void lineAcross(juce::Graphics& g, juce::Rectangle<float> r, float pos, juce::Slider& s) {
    const auto line = r.withSizeKeepingCentre(r.getWidth(), 4.0f);
    Ui::fillRounded(g, line, track, 2.0f);
    Ui::fillRounded(g, fillAcross(line, pos, s), fillColour(s), 2.0f);
    Ui::dot(g, { pos, r.getCentreY() }, 12.0f, text);
}

// How each linear style D uses is drawn; any other is JUCE's
struct StyleDrawer {
    juce::Slider::SliderStyle style;
    void (*draw)(juce::Graphics&, juce::Rectangle<float>, float, juce::Slider&);
};
constexpr StyleDrawer kDrawers[] = { { juce::Slider::LinearBar, barAcross },
                                     { juce::Slider::LinearBarVertical, barUp },
                                     { juce::Slider::LinearHorizontal, lineAcross } };

}

// A bar across or upright (UiFields.h; the MSM's steps), or a plain slider across, as kDrawers draws its style
// one-off: JUCE's LookAndFeel override; its parameters are JUCE's
void Feel::drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float min, float max,
                            juce::Slider::SliderStyle style, juce::Slider& s) {
    const auto* d = std::find_if(std::begin(kDrawers), std::end(kDrawers),
                                 [style](const StyleDrawer& k) { return k.style == style; });
    if (d == std::end(kDrawers)) {
        LookAndFeel_V4::drawLinearSlider(g, x, y, w, h, pos, min, max, style, s);
        return;
    }
    d->draw(g, juce::Rectangle<int>(x, y, w, h).toFloat(), pos, s);
}

// A switch: a pill, the accent when on, then its text; or, with the power property, a dot alone
void Feel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& b, bool, bool) {
    const bool on = b.getToggleState();
    auto r = b.getLocalBounds().toFloat();
    if (b.getProperties()[power]) {
        Ui::dot(g, r.getCentre(), 14.0f, on ? accent : track);
        return;
    }
    const auto pill = r.removeFromLeft(26.0f).withSizeKeepingCentre(26.0f, 14.0f);
    Ui::fillRounded(g, pill, on ? accent : track, 7.0f);
    const float knobX = on ? pill.getRight() - 7.0f : pill.getX() + 7.0f;
    Ui::dot(g, { knobX, pill.getCentreY() }, 10.0f, on ? ground : label);
    Ui::drawText(g, b.getButtonText(), r.withTrimmedLeft(8.0f).toNearestInt(),
                 { sans(11.0f, "Semibold"), b.isEnabled() ? text : muted });
}

// A button: `raised` on a panel, the accent when it is on
void Feel::drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down) {
    juce::Colour fill = b.getToggleState() ? accent : raised;
    if (highlighted || down) fill = fill.brighter(down ? 0.15f : 0.08f);
    Ui::fillRounded(g, b.getLocalBounds().toFloat(), b.isEnabled() ? fill : panel, 4.0f);
}

// A box: `raised`, its chevron in the names' colour at the right
// one-off: JUCE's LookAndFeel override; its parameters are JUCE's
void Feel::drawComboBox(juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) {
    const auto r = juce::Rectangle<float>((float)w, (float)h);
    Ui::fillRounded(g, r, box.isEnabled() ? raised : panel, 4.0f);
    const juce::Point<float> c(r.getRight() - 12.0f, r.getCentreY());
    juce::Path chevron;
    chevron.startNewSubPath(c.x - 3.0f, c.y - 1.5f);
    chevron.lineTo(c.x, c.y + 1.5f);
    chevron.lineTo(c.x + 3.0f, c.y - 1.5f);
    Ui::stroke(g, chevron, label, juce::PathStrokeType(1.5f));
}

void Feel::positionComboBoxText(juce::ComboBox& box, juce::Label& l) {
    l.setBounds(6, 0, box.getWidth() - 26, box.getHeight());
    l.setFont(getComboBoxFont(box));
}

}
