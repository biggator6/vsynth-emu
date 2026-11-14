#pragma once
// The keys' strip of a map of the zones (the Zones tab's, v195; D's ZONES page, UI step 4): each zone's keys as a band
// across a strip of the 128 keys, as a note picks its zone (xtZoneKeys), its number on it where it fits, and the C keys'
// names under it every other octave; in a look each editor gives.
#include "UiParts.h"
#include "XtSynthZones.h"

// How a strip is drawn: each zone's band's colour, the numbers' and the names' text, the bands' inset from the strip's
// top and bottom and their corners, and the names' height under the strip
struct ZoneStripLook {
    juce::Colour (*band)(int zone);
    Ui::TextStyle number, octave;
    int insetY;
    float radius;
    int octaveH;
};

void paintZoneStrip(juce::Graphics& g, juce::Rectangle<int> strip, const int (&upper)[kXtZones],
                    const ZoneStripLook& look);
