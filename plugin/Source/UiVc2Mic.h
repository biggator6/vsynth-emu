#pragma once
// The mic's checks (v453; research/UI_DESIGN_VC2.md, "6. The mic's state", d-12): why a VC-2 patch's vocoder is silent,
// each a line ticked or not, the first not ticked said in full: the Vocoder switch, the host's input, the mic's signal,
// a carrier note and the levels.  Those that read parameters and the processor's hasInput() are here; the mic's signal
// and a sounding note need Vc2Synth's view of its state (step 4.3), so they say they wait for it, ticked neither way.
// The header's MIC chip, the path's VOCODER chip and the VOCODER page's MIC IN card all read them.
#include "PluginProcessor.h"
#include <vector>

namespace Vc2Mic {

enum class State { ok, silent, waits };
struct Check {
    const char* name;   // its line's name: Vocoder, Input, Mic signal, Carrier, Level
    State state;
    juce::String why;   // what it says when it isn't ticked (silent), or what it waits for (waits)
};
// Every check in its order, as the parameters and the processor stand now
std::vector<Check> checks(VSynthEmuProcessor& p);
// The status line: the first silent check said in full; else "Vocoding" and the Type, with what still waits
juce::String status(VSynthEmuProcessor& p);
// The path's VOCODER chip's line: the Type when nothing known silences it, else the reason in a word (OFF, NO INPUT,
// LEVEL 0); and whether it is heard as far as the editor knows
juce::String chipWord(VSynthEmuProcessor& p);
bool heard(VSynthEmuProcessor& p);

}
