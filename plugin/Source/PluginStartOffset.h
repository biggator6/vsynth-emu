#pragma once
// Start Offset's text, the plugin's own (the user's d-7, 2026-11-27; research/NEXT_ISSUES.md item 5): the time a note
// plays from, where the XT shows the sample count.  The stored value stays the samples, a patch's word, so patches,
// sessions and automation are unchanged.  HARDWARE_FIRMWARE.md {#wave-edit-read}: the offset counts samples of the
// wave as the XT holds it, always 44.1 kHz, so the time is the value / 44100 on every wave.
#include <juce_audio_processors/juce_audio_processors.h>

// [MAN XT p. 116] "Data in the V-Synth XT is handled as 44.1 kHz data"
constexpr int kStartOffsetRate = 44100;

// Below a second in ms to two places ("250.00 ms": every sample its own text), from a second in s to three ("1.250 s")
juce::String startOffsetText(int samples);
// A typed time to the nearest sample, held to the parameter's range: "ms", "s" or "sec", or "smp" for samples as they
// are; a bare number is ms, as the box reads below a second
int startOffsetFromText(const juce::String& text);
// A Start Offset parameter's text both ways, the law above
juce::AudioParameterIntAttributes startOffsetAttributes();
