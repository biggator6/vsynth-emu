// D's ZONES page (UiZonesPage.h; UI step 4)
#include "UiZonesPage.h"
#include "PluginParamIds.h"
#include "UiFooter.h"
#include "UiLook.h"
#include "UiZoneStrip.h"

namespace {

constexpr int kStripH = 40, kOctaveH = 14, kKeysBodyH = kStripH + kOctaveH;
constexpr int kNameH = 18, kDotW = 10, kPerRow = kXtZones / 2, kTextH = 14, kPlaysLines = 3;

juce::StringArray splitIds() {
    juce::StringArray ids;
    for (int z = 0; z < kXtZones; ++z) ids.add(Params::zoneUpperId(z));
    return ids;
}

}

// The strip in D's look: each zone's band in its colour, rounded, its number in the ground's colour
void ZoneKeys::paint(juce::Graphics& g) {
    const auto strip = getLocalBounds().removeFromTop(kStripH);
    Ui::fillRounded(g, strip.toFloat(), Look::field, 4.0f);
    const ZoneStripLook look { Footer::zoneColour,
                               { Look::sans(11.0f, "Bold"), Look::ground, juce::Justification::centred, false },
                               { Look::mono(10.0f), Look::label }, 3, 3.0f, kOctaveH };
    paintZoneStrip(g, strip, upper, look);
}

ZoneCell::ZoneCell(Controls::State& state, int zone)
    : zone_(zone), upper_(state, Params::zoneUpperId(zone), "Range Upper", Footer::zoneColour(zone)) {
    addAndMakeVisible(upper_);
}

void ZoneCell::show(const XtZoneKeys& keys, const juce::String& plays) {
    keys_ = keys.lo <= keys.hi ? Ui::keyName(keys.lo) + " - " + Ui::keyName(keys.hi) : juce::String("no keys");
    plays_ = plays;
    repaint();
}

// Its colour and name over its bar; under it its keys, and what it plays in up to three lines
void ZoneCell::paint(juce::Graphics& g) {
    Look::paintPanel(g, getLocalBounds(), false);
    auto r = getLocalBounds().reduced(10, 8);
    auto name = r.removeFromTop(kNameH);
    Ui::dot(g, name.withWidth(kDotW).toFloat().getCentre(), (float)kDotW, Footer::zoneColour(zone_));
    Ui::drawText(g, "ZONE " + juce::String(zone_ + 1), name.withTrimmedLeft(16),
                 { Look::sans(12.0f, "Bold"), Look::text });
    r.removeFromTop(Bar::kHeight + 6);
    Ui::drawText(g, keys_, r.removeFromTop(kTextH), { Look::mono(10.0f), Look::text });
    g.setColour(Look::muted);
    g.setFont(Look::sans(11.0f));
    g.drawFittedText(plays_, r.withTrimmedTop(2), juce::Justification::topLeft, kPlaysLines);
}

void ZoneCell::resized() {
    upper_.setBounds(getLocalBounds().reduced(10, 8).withTrimmedTop(kNameH).withHeight(Bar::kHeight));
}

ZonesPage::ZonesPage(VSynthEmuProcessor& p)
    : processor_(p), splits_(p.apvts, splitIds(), [this] { refresh(); }) {
    keysCard_ = std::make_unique<Card>("KEYS");
    keys_ = &keysCard_->setBody(std::make_unique<ZoneKeys>());
    zonesCard_ = std::make_unique<Card>("ZONES");
    addAndMakeVisible(*keysCard_);
    addAndMakeVisible(*zonesCard_);
    for (int z = 0; z < kXtZones; ++z) {
        cells_[(size_t)z] = std::make_unique<ZoneCell>(p.apvts, z);
        zonesCard_->addAndMakeVisible(*cells_[(size_t)z]);
    }
    plays_.referTo(p.apvts.state.getPropertyAsValue("zonePlays", nullptr));
    plays_.addListener(this);
    refresh();
}

ZonesPage::~ZonesPage() {
    plays_.removeListener(this);
}

// What zone `zone` plays: zone 1 the panel's; the others the patch's as the processor says, or silent with no patch's data
juce::String ZonesPage::playsOf(const juce::StringArray& plays, int zone) {
    if (zone == 0) return "the panel";
    return zone < plays.size() && plays[zone].isNotEmpty() ? plays[zone] : juce::String("no patch, silent");
}

// The splits from their parameters onto the map and the cells, with what each zone plays (none while the page is being
// made: the watch's first call comes before them)
void ZonesPage::refresh() {
    if (keys_ == nullptr || !cells_.back()) return;
    for (int z = 0; z < kXtZones; ++z) keys_->upper[z] = juce::roundToInt(splits_.value((size_t)z));
    keys_->repaint();
    const juce::StringArray plays = processor_.zonePlays();
    for (int z = 0; z < kXtZones; ++z) cells_[(size_t)z]->show(xtZoneKeys(keys_->upper, z), playsOf(plays, z));
}

// The keys across the top, then the zones' cells in two rows of eight
void ZonesPage::resized() {
    auto r = getLocalBounds().reduced(Look::inset);
    keysCard_->setBounds(r.removeFromTop(keysCard_->heightFor(kKeysBodyH)));
    r.removeFromTop(Look::gap);
    zonesCard_->setBounds(r);
    auto body = zonesCard_->getLocalBounds().reduced(Card::kPad).withTrimmedTop(Card::kTitleH + Card::kGap);
    const int w = (body.getWidth() - (kPerRow - 1) * Look::gap) / kPerRow;
    const int h = (body.getHeight() - Look::gap) / 2;
    for (int z = 0; z < kXtZones; ++z) {
        const int x = body.getX() + (z % kPerRow) * (w + Look::gap);
        const int y = body.getY() + (z / kPerRow) * (h + Look::gap);
        cells_[(size_t)z]->setBounds(x, y, w, h);
    }
}
