// The keys' strip of a map of the zones (UiZoneStrip.h; moved from the Zones tab's ZoneMap, UI step 4)
#include "UiZoneStrip.h"

namespace {
constexpr int kKeys = 128, kOctaves = 24, kNameW = 40, kNumberFits = 14;
}

void paintZoneStrip(juce::Graphics& g, juce::Rectangle<int> strip, const int (&upper)[kXtZones],
                    const ZoneStripLook& look) {
    const auto xOf = [&strip](int key) { return strip.getX() + strip.getWidth() * key / kKeys; };
    for (int z = 0; z < kXtZones; ++z) {
        const XtZoneKeys k = xtZoneKeys(upper, z);
        if (k.lo > k.hi) continue;
        const juce::Rectangle<int> span(xOf(k.lo), strip.getY(), xOf(k.hi + 1) - xOf(k.lo), strip.getHeight());
        Ui::fillRounded(g, span.reduced(1, look.insetY).toFloat(), look.band(z), look.radius);
        if (span.getWidth() >= kNumberFits) Ui::drawText(g, juce::String(z + 1), span, look.number);
    }
    for (int key = 0; key < kKeys; key += kOctaves)
        Ui::drawText(g, Ui::keyName(key), { xOf(key), strip.getBottom(), kNameW, look.octaveH }, look.octave);
}
