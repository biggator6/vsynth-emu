#pragma once
// xt_vc2carrier's parts (v222): the carrier's (Vc2CarrierCheck.cpp), the VOCODER OSC's converter's and kind 5's
// update's (Vc2VocoderOscCheck.cpp), and the OSC block's converter's (Vc2OscConvertCheck.cpp, v251), on one
// interpreter.  XT_VC2CARRIER_MAIN only.
#include "Vc2CheckRig.h"

constexpr uint32_t kVc2OsClock = 0x893018d0;   // [FW] the OS's millisecond word, which the check's hook answers

// The second part's cases: what differs, counted (0 when nothing does); `clock` is the word the clock's hook answers
long vc2VocoderOscCases(Vc2Fw& fw, const Vc2Run& run, int32_t& clock);
// The third part's (v251), the same
long vc2OscConvertCases(Vc2Fw& fw, const Vc2Run& run);

// The converter parts' draws (v222's, here from v251, the arithmetic as it was): a float in [lo, hi)
inline float vc2Within(std::mt19937& rng, float lo, float hi) {
    return lo + (hi - lo) * (float)rng() / 4294967296.0f;
}

// A byte or halfword of a converter's input to draw: its offset and range
struct Vc2ByteDraw {
    int at, lo, hi;
};

// A byte of a record: an ID's range mostly, now and then any byte, which the loads' widths and signs must take
inline int vc2DrawByte(std::mt19937& rng, int lo, int hi) {
    return rng() % 8 == 0 ? (int)(rng() % 256) : lo + (int)(rng() % (uint32_t)(hi - lo + 1));
}

// A halfword into a converter's input, little-endian
inline void vc2Put16(uint8_t* at, int v) {
    const int16_t h = (int16_t)v;
    std::memcpy(at, &h, sizeof h);
}

// A converter's two LFO depths: their bytes in the input, and the LFO group's sums each is added to
struct Vc2LfoEdges {
    int depthAt[2];
    int sum[2];
};

// The LFO group's sums (v222's, here from v251): 0 half the time, else within ±reach; one draw in four the two sums the
// depths are added to put so that sum + depth lands on ±63 or ±64, a bound's edge, where a clamp's off-by-one shows
inline void vc2DrawLfoSums(std::mt19937& rng, const uint8_t* in, int16_t (&lfo)[5], int reach, const Vc2LfoEdges& e) {
    for (int16_t& v : lfo) v = rng() % 2 == 0 ? 0 : (int16_t)((int)(rng() % (uint32_t)(2 * reach + 1)) - reach);
    if (rng() % 4 != 0) return;
    constexpr int kEdges[4] = { 63, 64, -63, -64 };
    for (int k = 0; k < 2; ++k) lfo[e.sum[k]] = (int16_t)(kEdges[rng() % 4] - (int8_t)in[e.depthAt[k]]);
}

// rom.4's four Matrix getters for an OSC hooked (v222's, here from v251): its group's (0x893a23b0 OSC 1's, 0x893a2440
// OSC 2's) answer the address `oscAt` and its LFO's (0x893a26b0, 0x893a28b0) `lfoAt`, where a part draws the sums
inline void vc2HookGroups(Vc2Fw& fw, uint32_t oscAt, uint32_t lfoAt) {
    constexpr uint32_t kOscGetters[] = { 0x893a23b0, 0x893a2440 }, kLfoGetters[] = { 0x893a26b0, 0x893a28b0 };   // [FW]
    for (const uint32_t getter : kOscGetters) fw.cpu.hook(getter, [oscAt](Sh4& cpu) { cpu.ret(oscAt); });
    for (const uint32_t getter : kLfoGetters) fw.cpu.hook(getter, [lfoAt](Sh4& cpu) { cpu.ret(lfoAt); });
}
