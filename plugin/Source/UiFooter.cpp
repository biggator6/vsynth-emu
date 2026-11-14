// D's footer (UiFooter.h; UI step 1)
#include "UiFooter.h"
#include "PluginParamIds.h"
#include "UiLook.h"
#include "UiParts.h"

namespace {
constexpr int kOctave = 12, kWhitesPerOctave = 7, kBandH = 5;
constexpr int kKeys = Footer::kWhiteKeys / kWhitesPerOctave * kOctave;   // every key the keyboard shows
// Each semitone of an octave: the white key it is or follows, and whether it is black
constexpr int kWhiteOf[kOctave] = { 0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6 };
constexpr bool kBlack[kOctave] = { false, true, false, true, false, false, true, false, true, false, true, false };
// The zones' first colours (UI_DESIGN.md, "The look"); zones past them take lighter and darker steps of them
const juce::Colour kZoneBase[] = { Look::accent, Look::mod, juce::Colour(0xFFB9A3FF) };
const juce::Colour kWhiteKey { 0xFFC9CCD2 };

// Where a key's left edge is, in white keys from the keyboard's left
float whiteIndexOf(int key) {
    const int k = key - Footer::kLowKey;
    const int octave = k / kOctave, note = k % kOctave;
    return (float)(octave * kWhitesPerOctave + kWhiteOf[note]) + (kBlack[note] ? 0.65f : 0.0f);
}
}

juce::Colour Footer::zoneColour(int zone) {
    const int bases = (int)std::size(kZoneBase);
    const juce::Colour base = kZoneBase[zone % bases];
    const int step = zone / bases;
    return step % 2 ? base.darker(0.25f * (float)(step + 1) / 2.0f) : base.brighter(0.2f * (float)step / 2.0f);
}

void Footer::refresh() {
    juce::String now = processor_.apvts.getParameter(Params::kMonoPoly)->getCurrentValueAsText();
    for (int zone = 0; zone < processor_.patchZones(); ++zone)
        now << '|' << processor_.apvts.getRawParameterValue(Params::zoneUpperId(zone))->load();
    if (now == shown_) return;
    shown_ = now;
    repaint();
}

void Footer::paint(juce::Graphics& g) {
    g.fillAll(Look::header);
    Ui::fill(g, getLocalBounds().removeFromTop(1), Look::divider);
    auto r = getLocalBounds().reduced(Look::inset, 0).withTrimmedTop(4).withTrimmedBottom(6);
    const auto status = r.removeFromLeft(kStatusW);
    const juce::String keys = processor_.apvts.getParameter(Params::kMonoPoly)->getCurrentValueAsText();
    Ui::drawText(g, "Keys " + keys + "   Zones " + juce::String(processor_.patchZones()), status,
                 { Look::sans(11.0f), Look::label });
    r.removeFromLeft(10);
    const float keyW = (float)r.getWidth() / (float)kWhiteKeys;
    auto area = r.toFloat();
    paintZones(g, area.removeFromTop((float)kBandH), keyW);
    area.removeFromTop(2.0f);
    paintKeys(g, area, keyW);
}

// Each zone's keys, from the one past the zone before it to its Range Upper, as the XT finds a note's zone
void Footer::paintZones(juce::Graphics& g, juce::Rectangle<float> band, float keyW) const {
    const int highKey = kLowKey + kKeys - 1;
    int lower = 0;
    for (int zone = 0; zone < processor_.patchZones(); ++zone) {
        const int upper = juce::roundToInt(processor_.apvts.getRawParameterValue(Params::zoneUpperId(zone))->load());
        const int from = juce::jmax(lower, kLowKey), to = juce::jmin(upper, highKey);
        if (from <= to) {
            const float x0 = band.getX() + whiteIndexOf(from) * keyW;
            const float x1 = band.getX() + (whiteIndexOf(to) + 1.0f) * keyW;
            const juce::Rectangle<float> r { x0, band.getY(), juce::jmax(2.0f, x1 - x0 - 2.0f), band.getHeight() };
            Ui::fillRounded(g, r, zoneColour(zone), 2.0f);
        }
        lower = upper + 1;
    }
}

void Footer::paintKeys(juce::Graphics& g, juce::Rectangle<float> keys, float keyW) const {
    const float top = keys.getY(), blackW = keyW * 0.6f, blackH = keys.getHeight() * 0.6f;
    for (int i = 0; i < kWhiteKeys; ++i) {
        const float x = keys.getX() + (float)i * keyW;
        Ui::fillRounded(g, { x, top, keyW - 1.0f, keys.getHeight() }, kWhiteKey, 1.5f);
    }
    for (int k = kLowKey; k < kLowKey + kKeys; ++k)
        if (kBlack[k % kOctave])
            Ui::fillRounded(g, { keys.getX() + whiteIndexOf(k) * keyW, top, blackW, blackH }, Look::ground, 1.0f);
}
