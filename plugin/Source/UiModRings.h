#pragma once
// The knobs' modulation rings (v464; research/NEXT_ISSUES.md item 2, the user's d-5: a modulator shows as a ring beside
// the knob, which says what's stored; research/UI_DESIGN.md, "The look": a mod ring 3.2 outside the track over the
// modulation's reach).  A knob whose parameter its converter adds the Matrix's sum and the Multi Step Modulator's term
// to, straight and in the knob's own units (XtSynthKnobs.cpp, XtSynthEnvKnobs.cpp), is rung from its stored value to
// that plus the newest note's sum and term (XtSynthView), on the editor's timer: each OSC's Level, its LFO's Rate and
// four depths, the analog's PW and Fat LFO; each COSM's LFO Rate; the TVA's Level, Pan, their LFO depths, its LFO's
// Rate and the three sends; Portamento Time.  No ring with no note sounding or nothing added.  The editor's.
#include <juce_gui_basics/juce_gui_basics.h>
#include <map>

class VSynthEmuProcessor;

class ModRings {
public:
    explicit ModRings(VSynthEmuProcessor& p);   // its table, each OSC's and COSM's rows made by its index
    // Every knob shown under root rung as the synth's view says now; a knob's ring changed only when it moved
    void show(juce::Component& root);
    // A parameter's reach now, in its own units: what the newest note adds to it; 0 with none, or no row for it
    int reachOf(const juce::String& id) const;
    bool rings(const juce::String& id) const { return reach_.count(id) != 0; }

private:
    struct Reach {
        int slot = -1, dest = -1;   // the Matrix's slot (XtVoice::mx), the MSM's destination (XtVoice::msm); -1 none
    };
    VSynthEmuProcessor& processor_;
    std::map<juce::String, Reach> reach_;
};
