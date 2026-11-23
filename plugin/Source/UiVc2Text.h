#pragma once
// The VC-2's controls by rom.4's own value strings (v453; research/UI_DESIGN_VC2.md, d-8 and d-9): which kind of control
// an ID is, and a value's text in the editor where rom.4's routine has no format for it.  An ID's string (Vc2Params'
// vc2ValueString) is names split by a 1, a range's ends by a 2 and the unit after a 3; the routine (vc2ValueText) gives
// many of the VC-2's own IDs as their bare number where the string has another scale.  The editor shows those on the
// string's scale; the host parameters keep the routine's text (PluginVc2Params.cpp).
#include <juce_core/juce_core.h>

class XtFxValueText;

namespace Vc2Text {

enum class Kind { knob, box, toggle };
// d-8: an ID's control by its string: two names, OFF and ON or BYPASS and ON, a switch; other names, or a range beside a
// name in eight values or fewer, a box; a range a knob.  Without rom.4's tables every one is a stand-in, a knob.
Kind controlKind(int id, int lo, int hi);

// d-9: a value's text in the editor: COSM Type by the manual's short names (GROWL in COMB's place); the patch EQ's Freq
// and Q by rom.2's routine on the V-Synth's mid bands, which rom.4's driver puts them in (`vsynth`; bare without it); any
// other ID the routine's text where it gives its string's ends, else on its string's own scale
juce::String editorText(int id, int value, int lo, int hi, XtFxValueText* vsynth);

// The rom.4 ID a VC-2 host parameter is ("vc2z2c2_927" is 0x927), -1 if the host ID isn't one of the VC-2's
int romIdOf(const juce::String& paramId);

// The value a scale's text is, -1 if none: for a box's items (vc2's "CC17" in an assign is 17 as a controller)
int controllerOf(const juce::String& text);

}
