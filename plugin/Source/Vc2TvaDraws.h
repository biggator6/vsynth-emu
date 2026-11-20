#pragma once
// The TVA's words and block as xt_vc2tva draws them (v233), shared from v317 with xt_vc2tone, which draws the TVA in its
// tone by the same laws: never a NaN, an infinity or a denormal, which the interpreter's host floats and xtf's doubles
// may give otherwise.  Vc2TvaDraws.cpp, XT_VC2TVA_MAIN and XT_VC2TONE_MAIN only.
#include "Vc2Tva.h"
#include <array>
#include <cstdint>
#include <random>

inline constexpr uint32_t kTvaBlockWords = 0x50 / 4;

float tvaAboutUnit(std::mt19937& rng, float x);   // x, or a unit either side of it
// A float for a word the TVA multiplies, adds or compares: its usual range most often, its edges now and then
float tvaDrawFloat(std::mt19937& rng);
// A note-on's or a new block's block: noise, then the words the TVA reads, drawn as floats it can use
std::array<uint32_t, kTvaBlockWords> tvaDrawBlock(std::mt19937& rng);
void tvaDrawBlockFloats(uint32_t* block, std::mt19937& rng);   // those words alone, the rest of the block as it stands
uint32_t tvaDrawSwitch(std::mt19937& rng);   // a truncation's: clear most often, 1, or any
float tvaDrawFade(std::mt19937& rng);        // about the floor, 0, 1, or between
float tvaDrawGain(std::mt19937& rng);        // about the floor, at ±1, within them, or any
float tvaDrawTarget(std::mt19937& rng, float gain);   // at the gain or about it, or a gain's draw
// A step: one passing the target in the block exactly (the render's test at its edge), short of it, the rest over 32.5,
// or any
float tvaDrawStep(std::mt19937& rng, float target, float gain);
float tvaDrawElapsed(std::mt19937& rng);     // a whole number of renders most often
// The fade's step: one ending the fade within the block most often, one landing it on the floor itself, 0, below 0, or
// any
uint32_t tvaDrawFadeStep(std::mt19937& rng, const Vc2TvaWords& w);

// The words each routine decides by, each drawn one time in four (all of them when `all`), the bindings by `bind(w, rng)`
// every time, as the tone binds the TVA before its note-on (the constructor clears them, and rom.4 would read address 0
// on)
template <class Bind>
void tvaSteer(Vc2TvaWords& w, std::mt19937& rng, bool all, Bind bind) {
    const auto now = [&] { return all || rng() % 4 == 0; };
    if (now()) w.truncatedTold = tvaDrawSwitch(rng);
    if (now()) w.truncated = tvaDrawSwitch(rng);
    if (now()) w.fade = tvaDrawFade(rng);
    for (size_t c = 0; c < w.gain.size(); ++c) {
        if (now()) w.gain[c] = tvaDrawGain(rng);
        if (now()) w.target[c] = tvaDrawTarget(rng, w.gain[c]);
        if (now()) w.step[c] = tvaDrawStep(rng, w.target[c], w.gain[c]);
    }
    if (now()) w.elapsed = tvaDrawElapsed(rng);
    bind(w, rng);
    if (now()) {
        const std::array<uint32_t, kTvaBlockWords> block = tvaDrawBlock(rng);
        std::copy(block.begin(), block.end(), w.block.begin());
    }
}
