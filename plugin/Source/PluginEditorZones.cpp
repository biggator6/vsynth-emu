// The Zones tab (v195): the splits' sliders in two cards of eight, as the XT counts its zones, 1-8 and 9-16, and above them,
// across both columns, the map: the keys each zone plays (the part's table, xtZoneKeys, as a note picks its zone) and what each
// plays or why it is silent (the processor's zonePlays).  A page is 466 high at the smallest size: the map's card 148, a card
// of eight 274, with the gaps.
#include "PluginEditor.h"
#include "PluginParamIds.h"
#include "UiZoneStrip.h"

using Ui::Page;

namespace {

constexpr int kStripH = 34;      // the keys and their zones
constexpr int kOctaveH = 12;     // the C keys' names under them
constexpr int kLineCols = 4;     // the zones' lines, four a row
constexpr int kHalf = kXtZones / 2;

Ui::TextStyle numberStyle() {
    return { juce::Font(11.0f, juce::Font::bold), Ui::text, juce::Justification::centred, false };
}
Ui::TextStyle octaveStyle() { return { juce::Font(10.0f), Ui::label }; }
// A zone's band: one zone in two in a lighter shade
juce::Colour bandOf(int zone) { return Ui::accent.withAlpha(zone % 2 ? 0.35f : 0.65f); }
Ui::TextStyle lineStyle(bool reached) { return { juce::Font(11.0f), reached ? Ui::text : Ui::line }; }

} // namespace

void ZoneMap::paint(juce::Graphics& g) {
    auto a = getLocalBounds();
    paintKeys(g, a.removeFromTop(kStripH + kOctaveH));
    a.removeFromTop(4);
    paintLines(g, a);
}

// Each zone's keys as a band, one zone in two in a lighter shade, its number on it where it fits; under them the C keys'
// names, every other octave
void ZoneMap::paintKeys(juce::Graphics& g, juce::Rectangle<int> area) const {
    const auto strip = area.removeFromTop(kStripH);
    Ui::fill(g, strip, Ui::surfaceField);
    const ZoneStripLook look { bandOf, numberStyle(), octaveStyle(), 0, 0.0f, kOctaveH };
    paintZoneStrip(g, strip, upper, look);
}

// A line a zone, zones 1-4 down the first column: its number, its keys and what it plays, or that no key reaches it
void ZoneMap::paintLines(juce::Graphics& g, juce::Rectangle<int> area) const {
    const int rows = kXtZones / kLineCols;
    const int w = area.getWidth() / kLineCols, h = area.getHeight() / rows;
    for (int z = 0; z < kXtZones; ++z) {
        const juce::Rectangle<int> cell(area.getX() + w * (z / rows), area.getY() + h * (z % rows), w - 6, h);
        const XtZoneKeys k = xtZoneKeys(upper, z);
        const bool reached = k.lo <= k.hi;
        const juce::String keys = reached ? Ui::keyName(k.lo) + "-" + Ui::keyName(k.hi) + "  " + linesOf(z) : "no keys";
        Ui::drawText(g, juce::String(z + 1) + "  " + keys, cell, lineStyle(reached));
    }
}

// What zone `zone` plays: zone 1 the panel's; the others the patch's, as the processor says, or silent with no patch
juce::String ZoneMap::linesOf(int zone) const {
    if (zone == 0) return "the panel";
    if (zone < plays.size() && plays[zone].isNotEmpty()) return plays[zone];
    return "no patch, silent";
}

void ZonesControls::make(Apvts& apvts) {
    for (int z = 0; z < kXtZones; ++z) {
        juce::Slider& s = upper[(size_t)z];
        s.setSliderStyle(juce::Slider::LinearHorizontal);
        s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
        Ui::styleValueSlider(s, 40);                 // "127" whole
        s.onValueChange = [this] { refresh(); };
        names[(size_t)z].setText("Zone " + juce::String(z + 1), juce::dontSendNotification);
        atts.push_back(std::make_unique<Apvts::SliderAttachment>(apvts, Params::zoneUpperId(z), s));
    }
    // The last zone's split sets nothing: the part's table ends it at 127 whatever it says [FW 0x894794f0], so its slider
    // is greyed, and the map's title says why
    upper[kXtZones - 1].setEnabled(false);
    plays.referTo(apvts.state.getPropertyAsValue("zonePlays", nullptr));
    refresh();
}

void ZonesControls::refresh() {
    for (int z = 0; z < kXtZones; ++z) map.upper[z] = juce::roundToInt(upper[(size_t)z].getValue());
    map.plays = juce::StringArray::fromLines(plays.toString());
    map.repaint();
}

// The controls styled and on their parameters, on the editor so the page adopts them; then the cards: the map across both
// columns, zones 1-8 in column 0 and 9-16 in column 1
void VSynthEmuEditor::buildZonesPage() {
    zones_.make(processor_.apvts);
    for (int z = 0; z < kXtZones; ++z) {
        styleSmall(zones_.names[(size_t)z]);
        addAndMakeVisible(zones_.names[(size_t)z]);
        addAndMakeVisible(zones_.upper[(size_t)z]);
    }
    addAndMakeVisible(zones_.map);
    Page& p = *pages_[kZonesTab];
    Card map { "Zones and their keys: zone 16 always ends at G9", {}, {}, -1 };
    map.customH = ZonesControls::kMapH;
    map.custom = [this](juce::Rectangle<int> a) { zones_.map.setBounds(a); };
    map.extra = { &zones_.map };
    p.add(map);
    for (int half = 0; half < 2; ++half) {
        std::vector<Page::Row> rows;
        for (int z = kHalf * half; z < kHalf * (half + 1); ++z)
            rows.push_back(lr(zones_.names[(size_t)z], zones_.upper[(size_t)z]));
        p.add(Card { half ? "Zones 9-16" : "Zones 1-8", {}, rows, half });
    }
}
