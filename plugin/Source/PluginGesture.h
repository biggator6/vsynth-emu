#pragma once
// A parameter set to a value in its own units as a control sets it, a whole gesture: the editor's (Controls::setValue)
// and the MIDI edits' (PluginMidiEdits::flush, v462).  Message thread.
#include <juce_audio_processors/juce_audio_processors.h>

inline void setAsGesture(juce::RangedAudioParameter& p, float value) {
    p.beginChangeGesture();
    p.setValueNotifyingHost(p.convertTo0to1(value));
    p.endChangeGesture();
}
