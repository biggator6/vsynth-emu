#pragma once
// The VC-2 mode's rack for a zone (v453; research/UI_DESIGN_VC2.md, "SYNTH"): D's rack (UiRack.h) of D's panels made from
// the VC-2's tables: an OSC panel with a face for each OSC Type (ANALOG, PCM, EXTIN, VOCODER), a COSM panel with one for
// each of rom.4's seventeen COSM Types, AMP's five knobs, and the Time Trip column on the patch's Pad Mode and Pad Hold,
// its pad sending the pair Pad Mode names (d-10).  Every control is made for the zone by Vc2Blocks::param.
#include "UiRack.h"

namespace Vc2Rack {

Rack::Panels panels(VSynthEmuProcessor& p, int zone);
// The controllers the pad sends, X then Y, by Pad Mode: Pad Assign X and Y under X-Y, Time Trip Assign X and Y under
// TIME-TRIP; 0 for an assign that names no controller
juce::Point<int> vc2PadAssigns(juce::AudioProcessorValueTreeState& state);

}
