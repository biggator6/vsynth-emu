// D's footer (UiFooter.h; UI step 1, its keys played from v201)
#include "UiFooter.h"
#include "PluginParamIds.h"
#include "UiLook.h"
#include "UiParts.h"
#include <algorithm>
#include <ranges>

namespace {
constexpr int kOctave = 12, kWhitesPerOctave = 7, kBandH = 5, kBandGap = 2, kStatusGap = 10;
constexpr int kKeys = Footer::kWhiteKeys / kWhitesPerOctave * kOctave;   // every key the keyboard shows
constexpr float kBlackW = 0.6f, kBlackH = 0.6f;   // a black key: its share of a white key's width, of the keys' height
constexpr float kSoftest = 0.25f;                 // a press's velocity at a key's top edge; full at its bottom
constexpr int kChannel = 1;
// Each semitone of an octave: the white key it is or follows, and whether it is black
constexpr int kWhiteOf[kOctave] = { 0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6 };
constexpr bool kBlack[kOctave] = { false, true, false, true, false, false, true, false, true, false, true, false };
// The zones' first colours (UI_DESIGN.md, "The look"); zones past them take lighter and darker steps of them
const juce::Colour kZoneBase[] = { Look::accent, Look::mod, juce::Colour(0xFFB9A3FF) };
const juce::Colour kWhiteKey { 0xFFC9CCD2 };

bool isBlack(int key) { return kBlack[key % kOctave]; }

// Where a key's left edge is, in white keys from the keyboard's left
float whiteIndexOf(int key) {
    const int k = key - Footer::kLowKey;
    const int octave = k / kOctave, note = k % kOctave;
    return (float)(octave * kWhitesPerOctave + kWhiteOf[note]) + (kBlack[note] ? 0.65f : 0.0f);
}
}

Footer::~Footer() {
    letGo();
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

// The keys' area: right of the status, under the zones' band
juce::Rectangle<float> Footer::keysArea() const {
    auto r = getLocalBounds().reduced(Look::inset, 0).withTrimmedTop(4).withTrimmedBottom(6);
    r.removeFromLeft(kStatusW + kStatusGap);
    return r.toFloat().withTrimmedTop((float)(kBandH + kBandGap));
}

juce::Rectangle<float> Footer::keyBounds(int key) const {
    const auto keys = keysArea();
    const float keyW = keys.getWidth() / (float)kWhiteKeys, x = keys.getX() + whiteIndexOf(key) * keyW;
    if (isBlack(key)) return { x, keys.getY(), keyW * kBlackW, keys.getHeight() * kBlackH };
    return { x, keys.getY(), keyW - 1.0f, keys.getHeight() };
}

std::optional<int> Footer::keyAt(juce::Point<float> at) const {
    const auto keys = std::views::iota(kLowKey, kLowKey + kKeys);
    for (const bool black : { true, false }) {
        const auto under = [&](int k) { return isBlack(k) == black && keyBounds(k).contains(at); };
        const auto it = std::ranges::find_if(keys, under);
        if (it != keys.end()) return *it;
    }
    return std::nullopt;
}

float Footer::velocityAt(int key, float y) const {
    const auto b = keyBounds(key);
    return kSoftest + (1.0f - kSoftest) * juce::jlimit(0.0f, 1.0f, (y - b.getY()) / b.getHeight());
}

void Footer::mouseDown(const juce::MouseEvent& e) {
    press(e.position);
}

void Footer::mouseDrag(const juce::MouseEvent& e) {
    if (keyAt(e.position) != held_) press(e.position);
}

void Footer::mouseUp(const juce::MouseEvent&) {
    letGo();
}

void Footer::press(juce::Point<float> at) {
    letGo();
    held_ = keyAt(at);
    if (!held_) return;
    processor_.sendFromEditor(juce::MidiMessage::noteOn(kChannel, *held_, velocityAt(*held_, at.y)));
    repaint(keyBounds(*held_).getSmallestIntegerContainer());
}

void Footer::letGo() {
    if (!held_) return;
    processor_.sendFromEditor(juce::MidiMessage::noteOff(kChannel, *held_));
    repaint(keyBounds(*held_).getSmallestIntegerContainer());
    held_.reset();
}

void Footer::paint(juce::Graphics& g) {
    g.fillAll(Look::header);
    Ui::fill(g, getLocalBounds().removeFromTop(1), Look::divider);
    const auto status = getLocalBounds().reduced(Look::inset, 0).withTrimmedTop(4).withTrimmedBottom(6)
                            .removeFromLeft(kStatusW);
    const juce::String keys = processor_.apvts.getParameter(Params::kMonoPoly)->getCurrentValueAsText();
    Ui::drawText(g, "Keys " + keys + "   Zones " + juce::String(processor_.patchZones()), status,
                 { Look::sans(11.0f), Look::label });
    const auto area = keysArea();
    const float keyW = area.getWidth() / (float)kWhiteKeys;
    paintZones(g, area.withY(area.getY() - (float)(kBandH + kBandGap)).withHeight((float)kBandH), keyW);
    paintKeys(g);
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

// The white keys, then the black over them; the held one in the accent
void Footer::paintKeys(juce::Graphics& g) const {
    for (const bool black : { false, true })
        for (int k = kLowKey; k < kLowKey + kKeys; ++k) {
            if (isBlack(k) != black) continue;
            const juce::Colour face = held_ == k ? Look::accent : black ? Look::ground : kWhiteKey;
            Ui::fillRounded(g, keyBounds(k), face, black ? 1.0f : 1.5f);
        }
}
