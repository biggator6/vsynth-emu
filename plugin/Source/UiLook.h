#pragma once
// D's look (research/UI_DESIGN.md, "The redesign, D": "The look"): its palette, the drawing's size and regions, its type,
// and the LookAndFeel that draws knobs, switches, boxes and buttons in it (UI step 1).
#include <juce_gui_basics/juce_gui_basics.h>

namespace Look {

inline const juce::Colour ground  { 0xFF141518 };   // window, page bar
inline const juce::Colour header  { 0xFF0F1012 };   // header, footer
inline const juce::Colour panel   { 0xFF1C1E22 };   // rack panels, path blocks, the header's boxes
inline const juce::Colour raised  { 0xFF24272D };   // buttons and boxes on a panel
inline const juce::Colour lower   { 0xFF1A1C20 };   // the lower panel
inline const juce::Colour field   { 0xFF0F1012 };   // displays: the wave, curves, the pad, routes
inline const juce::Colour edge    { 0xFF2C2F36 };   // a panel's border
inline const juce::Colour divider { 0xFF2A2D33 };   // lines between regions, the selected list entry, a big knob's cap
inline const juce::Colour track   { 0xFF33363D };   // a knob's track, an outlined button
inline const juce::Colour text    { 0xFFECEDEF };   // values, names on panels
inline const juce::Colour label   { 0xFFA3A8B2 };   // a control's name
inline const juce::Colour muted   { 0xFF8E949F };   // hints, secondary text
inline const juce::Colour dim     { 0xFF6B717C };   // arrows and marks, never text
inline const juce::Colour accent  { 0xFFF5A524 };   // value arcs, the selected block, what is on
inline const juce::Colour mod     { 0xFF46C2E0 };   // modulation, LFO curves, Matrix routes, the Time Trip
inline const juce::Colour ready   { 0xFF5FD28B };   // the ROM's state when it is loaded
inline const juce::Colour warning { 0xFFE8743B };   // the ROM's state when it isn't

// The drawing's own size, scaled whole between these (UI_DESIGN.md, "The window")
inline constexpr int width = 1200, height = 750;
inline constexpr float minScale = 0.8f, maxScale = 1.2f;
// The regions, top to bottom, and the rack's and lower panel's insets
inline constexpr int headerH = 48, pageBarH = 34, pathH = 52, rackH = 304, lowerH = 272, footerH = 40;
inline constexpr int inset = 12, gap = 8, radius = 8;

// The names' face and the values' face, at a size in the drawing's units (a CSS px of the mock-up)
juce::Font sans(float size, const char* style = "Regular", float spacing = 0.0f);
juce::Font mono(float size);

// Where a knob's sweep starts and ends: 135° to 405° in the mock-up's angles, from 12 o'clock as JUCE counts them
inline constexpr float sweepStart = 1.25f * juce::MathConstants<float>::pi;
inline constexpr float sweepEnd = 2.75f * juce::MathConstants<float>::pi;

// A slider's properties a knob's modulation ring reads: where the modulation reaches, as proportions of the range
inline const juce::Identifier modLo { "modLo" }, modHi { "modHi" };
// A toggle drawn as a power dot rather than a switch, by this property
inline const juce::Identifier power { "power" };

class Feel : public juce::LookAndFeel_V4 {
public:
    Feel();
    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end,
                          juce::Slider&) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted,
                              bool down) override;
    juce::Font getTextButtonFont(juce::TextButton&, int) override { return sans(11.0f, "Bold"); }
    void drawComboBox(juce::Graphics&, int w, int h, bool down, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override { return sans(11.0f, "Semibold"); }
    void positionComboBoxText(juce::ComboBox&, juce::Label&) override;
    juce::Font getLabelFont(juce::Label& l) override { return l.getFont(); }
};

}
