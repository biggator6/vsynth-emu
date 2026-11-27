#pragma once
// A processor's file buffer, one per OSC: moved as it stood out of PluginProcessor.h (v452), which was past the kit's
// line limit, as VSynthEmuProcessor::LoadedWave.  Written on the message thread, OSC 1's read on the audio thread
// (legacy path).
#include <juce_audio_basics/juce_audio_basics.h>
#include "PluginWaveEdit.h"
#include "XtFiles.h"
#include <atomic>
#include <memory>

struct PluginLoadedWave {
    juce::AudioBuffer<float> buffer;
    std::atomic<bool>        loaded { false };
    juce::String             name;
    double                   rate = 44100.0;
    int                      bits = 16;
    int                      ownType = -1;       // a V-Synth wave's own encode type (XtVpwFormat): 0 LITE, 1 SOLO, 3 BACKING, 5 ENSEMBLE; -1 none
    bool                     isOwn = false;      // a V-Synth wave, and what it says of itself (its audio left out)
    XtWaveFile               own;
    std::atomic<int>         gen { 0 };          // a newer encode request discards an older result
    std::atomic<bool>        reencode { false }; // its Encode Type or Depth changed
    // v479: what the session keeps of it beyond its file (the WAVE page's edits, put on at each encode), and the wave as
    // last encoded with them, as the synth plays it; both under the processor's bufferLock_
    PluginWaveEdit           edit;
    std::shared_ptr<const XtEncoded> encoded;
};
