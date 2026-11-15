#pragma once
// VSynthEmuExtInTest's parts (v207): the feed alone in ExtInTest.cpp, the processor in ExtInPluginTest.cpp
#include "TestChecks.h"
#include <juce_core/juce_core.h>
#include <cstdint>

// The processor with an EXT IN OSC against XtSynth given the input by the law, with the Effects off and on; its layouts,
// a mono input, none, and another rate
void extInPluginChecks(TestChecks& check, const juce::File& rom);

// A host's input, the same each run: frame `frame`'s side `side` as a 24-bit sample over the whole range, so that its
// word at the XT's full scale is exact (k × 2^8)
inline int32_t hostSample24(long frame, int side) {
    uint64_t z = (uint64_t)(2 * frame + side) + 0x9e3779b97f4a7c15ull;   // splitmix64 of the frame and side
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    return (int32_t)(uint32_t)(z ^ (z >> 31)) >> 8;
}
inline float hostSample(long frame, int side) { return (float)hostSample24(frame, side) * 0x1p-23f; }
