#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

// `seconds` of a line of notes at 44.1 kHz, mono, a semitone up every half second with a little noise: something the
// encoders work at.  Written to wave_<seconds>s.wav in `folder`; an empty File if it can't be.
juce::File writeTestWave(const juce::File& folder, int seconds);
// `audio` written to `file` as a WAV at `rate`, `bits` a sample (32: floats, as they are); false if it can't be
bool writeWav(const juce::File& file, const juce::AudioBuffer<float>& audio, int rate, int bits);
