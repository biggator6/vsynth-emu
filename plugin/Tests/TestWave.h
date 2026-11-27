#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

// `seconds` of a line of notes at 44.1 kHz, mono, a semitone up every half second with a little noise: something the
// encoders work at.  Written to wave_<seconds>s.wav in `folder`; an empty File if it can't be.
juce::File writeTestWave(const juce::File& folder, int seconds);
// `audio` written to `file` as a WAV at `rate`, `bits` a sample (32: floats, as they are); false if it can't be
bool writeWav(const juce::File& file, const juce::AudioBuffer<float>& audio, int rate, int bits);
// A V-Synth SOLO wave as a project keeps one, `frames` of a steady sine (EditorWaveCutTest.cpp, v480; here from v481):
// a RIFF with its sample chunk (type SOLO, mono, its frames, key 60), its loop record (both loops the whole wave, One
// Way, the Loop switch deciding), plain samples at 44.1 kHz 16-bit, and, with `events` (in 1/16 sample, the first 0 and
// the last the end), a vpdt holding them where a SOLO wave's are and nothing else; written to `name` in `folder`
juce::File writeSoloWave(const juce::File& folder, const juce::String& name, long frames,
                         const std::vector<int32_t>& events = {});
