#pragma once
// A loaded wave's edits from the WAVE page (v479; research/NEXT_ISSUES.md item 6, the user's d-8: the session's copy
// first), the points as XtSynth plays them.  The forward loop is the OSC's Loop Start and End, a share of the wave's
// last frame (XtSynth::waveLoop), where a V-Synth wave's own loop lands as it loads; set from a frame here so that it
// gives that frame back.  The backward loop is the wave's own (the loop record's +0x10, +0xc and +0x19, XtEncoded's
// bwd), which an edit replaces and the session keeps.  HARDWARE_FIRMWARE.md {#wave-edit-read}: the XT keeps both loops
// in a wave's vplp, the shortest 16 samples.
#include "XtEncoded.h"
#include <juce_audio_processors/juce_audio_processors.h>

struct PluginWaveEdit {
    bool backward = false;                       // the backward loop is the session's own, not the wave's
    int bwdMode = 1, bwdStart = 0, bwdEnd = 0;   // its Loop Mode (0 One Shot, 1 One Way, 2 Alternate), region in frames
    bool operator==(const PluginWaveEdit&) const = default;
};

// The loop's shortest, in frames between its ends, as XtSynth::waveLoop holds it
constexpr int kWaveLoopShortest = 16;

// A loop end in frames from the OSC's Loop Start or End parameter's value (a percent), as XtSynth takes it: the
// share, read as xtControls does (÷ 100 in single precision), of the last frame, rounded
int waveLoopFrame(float percent, long frames);
// The value of a Loop Start or End parameter that gives `frame` back through `p` (as the host's value round trips it)
// and waveLoopFrame: the nearest float that does.  Every frame of a wave up to 2^23 frames (190 s) has one; past that the
// shares a float holds near the wave's end are more than a frame apart, and it gives the nearest frame it can
float waveLoopPercentFor(const juce::RangedAudioParameter& p, int frame, long frames);

// The edit onto a wave just encoded, after its own (the encode job's)
void waveEditOnto(const PluginWaveEdit& edit, XtEncoded& e);

// What the WAVE page edits of an OSC's wave, through the processor (its friend); the message thread's
class VSynthEmuProcessor;
struct PluginWaveEdits {
    // The backward loop as the synth plays it: the session's edit, the wave's own, or none (the forward loop's)
    static PluginWaveEdit backward(const VSynthEmuProcessor& p, int osc);
    // The session's own edit of the wave, none if it keeps none
    static PluginWaveEdit edit(const VSynthEmuProcessor& p, int osc);
    // An edit of the backward loop, kept in the session and given to the synth by the wave's encode, which puts it on
    // after the wave's own as a session's restore does (an encoded wave can't be copied: its band player reads its own)
    static void setBackward(VSynthEmuProcessor& p, int osc, const PluginWaveEdit& edit);
    // None kept, as a new wave comes (a file loaded, a patch's waves); and the session's words for them, both ways, by
    // the state's properties "waveEdit" and "waveEdit2" (a restore puts them back before it loads the waves)
    static void forget(VSynthEmuProcessor& p, int osc);
    static void toState(const VSynthEmuProcessor& p, juce::ValueTree& state);
    static void fromState(VSynthEmuProcessor& p, const juce::ValueTree& state);
};
// The session's words for it, both ways; none for no edit
juce::String waveEditText(const PluginWaveEdit& edit);
PluginWaveEdit waveEditFromText(const juce::String& text);
