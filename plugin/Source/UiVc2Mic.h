#pragma once
// The mic's checks (v453; research/UI_DESIGN_VC2.md, "6. The mic's state", d-12): why a VC-2 patch's vocoder is silent,
// each a line ticked or not, the first not ticked said in full: the Vocoder switch, the host's input, the mic's signal,
// a carrier note and the levels.  The mic's signal and a sounding note read the playing part's view (v456,
// Vc2PartView.h): the input under −60 dBFS for a second is NO MIC, no voice sounding NO NOTE; while no part has played
// they say they wait for it, ticked neither way.  And the MIC IN card's state lines from the same view.  The header's
// MIC chip, the path's VOCODER chip and the VOCODER page's MIC IN card all read them.
#include "PluginProcessor.h"
#include <vector>

namespace Vc2Mic {

enum class State { ok, silent, waits };
struct Check {
    const char* name;   // its line's name: Vocoder, Input, Mic signal, Carrier, Level
    State state;
    juce::String why;   // what it says when it isn't ticked (silent), or what it waits for (waits)
};
// Every check in its order, as the parameters, the processor and the part's view stand now; the mic's and the
// carrier's places in it
std::vector<Check> checks(VSynthEmuProcessor& p);
constexpr size_t kMicCheck = 2, kCarrierCheck = 3;
// v456: the MIC IN card's state, a line each in the card's order (the input's level, VOICED or UNVOICED, the tracked
// pitch and its lock, Auto Note's note, the notes sounding, the output's level), each saying it waits while no part
// has played
constexpr size_t kStateLines = 6;
std::vector<juce::String> stateLines(VSynthEmuProcessor& p);
// The status line: the first silent check said in full; else "Vocoding" and the Type, with what still waits
juce::String status(VSynthEmuProcessor& p);
// The path's VOCODER chip's line: the Type when nothing known silences it, else the reason in a word (OFF, NO INPUT,
// LEVEL 0); and whether it is heard as far as the editor knows
juce::String chipWord(VSynthEmuProcessor& p);
bool heard(VSynthEmuProcessor& p);

}
