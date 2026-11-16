// xt_vc2carrier <rom.4> [cases] [seed] (v221): the VOCODER OSC's carrier, Vc2Carrier, against rom.4's own routines on a
// carrier in Sh4Interp's RAM, after the carriers' rate setter (0x893ec6a0, the static initializer that writes the pitch
// factor 0x899d9964), with the OS clock (0x893018d0) answered by a drawn word.  Each case the init (0x893ebed0) from a
// drawn P and clock, then blocks of the render (0x893ebfa0), with now and then before one the update (0x893ec4d0) or
// the settings (0x893ec450), each on a drawn P or none, the input stage's peak drawn into 0x899d9a5c.  P's levels are a
// parameter's v/127, its pitch 19 Hz…10 kHz with the update's bounds now and then, Growl off half the time, the
// Dynamics off a third.  After every routine the carrier's words (+0x00…+0x68), after a render its frames too, word
// for word.  Then its second part (v222, Vc2VocoderOscCheck.cpp): the VOCODER OSC's converter's own part and kind 5's
// update.  XT_VC2CARRIER_FIRST=1 says where the first case to differ does, XT_VC2CARRIER_STOP=1 stops there.
// XT_VC2CARRIER_MAIN only.
#ifdef XT_VC2CARRIER_MAIN
#include "Vc2Carrier.h"
#include "Vc2CarrierCheck.h"
#include "XtSwitches.h"
#include <bit>
#include <cmath>
#include <cstdio>

namespace {
constexpr uint32_t kSetRate = 0x893ec6a0, kInit = 0x893ebed0, kSettings = 0x893ec450, kUpdate = 0x893ec4d0;  // [FW]
constexpr uint32_t kRender = 0x893ebfa0, kPeakAt = 0x899d9a5c;                                               // [FW]
constexpr uint32_t kCarrierAt = 0x8C040000, kParamsAt = 0x8C041000, kFramesAt = 0x8C036000;   // in Vc2Fw's RAM
constexpr int kBlocks = 120, kEventOneIn = 6, kFrameWords = 2 * Vc2Carrier::kFrames;

struct Counts {
    long blocks = 0, growling = 0, updates = 0, settings = 0, none = 0, dynamics = 0;
};


// A pitch: 19 Hz…10 kHz, with 1 and the high-pass's bounds now and then, and the growl divider's edges, where the
// step × 440 ÷ 50 is a whole number n and the order of the divider's rounding decides n or n − 1
float drawPitch(std::mt19937& rng) {
    constexpr float kPitches[7] = { 1.0f, 0.25f, 2.0f, 0.0f, -1.0f, -1.0f, -1.0f };   // 0: an edge; -1: any
    const float pitch = kPitches[rng() % 7];
    if (pitch > 0.0f) return pitch;
    if (pitch == 0.0f) return (float)(2 + rng() % 199) * 50.0f / 440.0f;
    return std::exp2(9.0f * vc2Unit(rng) - 4.5f);
}

// A P as kind 5's update leaves it: levels as a parameter's v/127; the pitch drawPitch's; Growl off half the time;
// the Dynamics off a third (now and then their floor itself); the balance's centre now and then; +0x08, which the
// carrier doesn't read, noise
Vc2CarrierParams drawCarrierParams(std::mt19937& rng) {
    Vc2CarrierParams p {};
    p.gain = rng() % 8 == 0 ? 0.0f : 1.2f * vc2Unit(rng);
    p.pitch = drawPitch(rng);
    p.notRead = vc2Unit(rng);
    for (float* level : { &p.sawCutoff, &p.sawLevel, &p.noiseLowpass, &p.noiseHighpass, &p.noiseLevel, &p.sensitivity })
        *level = vc2DrawLevel(rng);
    p.growl = rng() % 2 == 0 ? 0.0f : (rng() % 4 == 0 ? 1.0f : vc2Unit(rng));
    const int dynamics = (int)(rng() % 6);
    p.threshold = dynamics < 2 ? 0.0f : dynamics == 2 ? vc2CarrierTables().floor : vc2DrawLevel(rng);
    p.balance = rng() % 6 == 0 ? 1.0f : (float)(rng() % 128) / 64.0f;
    return p;
}

// The input stage's peak: 0 now and then, else mostly low
float drawPeak(std::mt19937& rng) { return rng() % 4 == 0 ? 0.0f : 1.5f * vc2Unit(rng) * vc2Unit(rng); }

std::string carrierDifference(Vc2Fw& fw, const Vc2Carrier& c) {
    return vc2FirstDifference(fw, kCarrierAt, &c.w, sizeof c.w, "the carrier's words");
}

// The carrier's two routines on P or none: rom.4's on (the carrier, P or 0), the port's on P or nullptr and the peak
struct ParamsEvent {
    uint32_t routine;
    const char* name;
    void (*port)(Vc2Carrier&, const Vc2CarrierParams*, float);
    long Counts::*count;
};
constexpr ParamsEvent kParamsEvents[] = {
    { kUpdate, "the update", [](Vc2Carrier& c, const Vc2CarrierParams* p, float peak) { c.update(p, peak); },
      &Counts::updates },
    { kSettings, "the settings", [](Vc2Carrier& c, const Vc2CarrierParams* p, float) { c.settings(p); },
      &Counts::settings },
};

std::string paramsEventOn(Vc2Fw& fw, Vc2Carrier& c, std::mt19937& rng, Counts& n) {
    const ParamsEvent& e = kParamsEvents[rng() % std::size(kParamsEvents)];
    const Vc2CarrierParams p = drawCarrierParams(rng);
    const bool none = rng() % 8 == 0;
    const float peak = drawPeak(rng);
    ++(n.*e.count);
    n.none += none ? 1 : 0;
    n.dynamics += e.routine == kUpdate && !(vc2CarrierTables().floor > c.w.threshold) ? 1 : 0;
    fw.cpu.write(kParamsAt, &p, sizeof p);
    fw.cpu.wr32(kPeakAt, std::bit_cast<uint32_t>(peak));
    return vc2BothOn(fw, e.routine, { kCarrierAt, none ? 0u : kParamsAt }, e.name,
                     [&] { e.port(c, none ? nullptr : &p, peak); }, [&] { return carrierDifference(fw, c); });
}

std::string renderOn(Vc2Fw& fw, Vc2Carrier& c, Counts& n) {
    ++n.blocks;
    n.growling += c.w.growl > 0.0f ? 1 : 0;
    float frames[kFrameWords] = {};
    fw.cpu.write(kFramesAt, frames, sizeof frames);
    const std::string what = vc2BothOn(fw, kRender, { kCarrierAt, kFramesAt }, "the render", [&] { c.render(frames); },
                                       [&] { return carrierDifference(fw, c); });
    return what.empty() ? vc2FirstDifference(fw, kFramesAt, frames, sizeof frames, "the frames") : what;
}

// A case's init on both, the clock the hook answers drawn (now and then a short of −32768, the magnitude's edge)
std::string initOn(Vc2Fw& fw, Vc2Carrier& c, std::mt19937& rng, int32_t& clock) {
    const Vc2CarrierParams p = drawCarrierParams(rng);
    clock = rng() % 16 == 0 ? (int32_t)0x12348000 : (int32_t)rng();
    fw.cpu.write(kParamsAt, &p, sizeof p);
    return vc2BothOn(fw, kInit, { kCarrierAt, kParamsAt }, "the init", [&] { c.init(p, clock); },
                     [&] { return carrierDifference(fw, c); });
}
} // namespace

int main(int argc, char** argv) {
    Vc2Fw fw;
    const auto run = vc2Start(argc, argv, fw, "xt_vc2carrier", vc2LoadCarrier);
    if (!run) return 1;
    int32_t clock = 0;
    fw.cpu.hook(kVc2OsClock, [&clock](Sh4& cpu) { cpu.ret((uint32_t)clock); });
    if (!fw.call(kSetRate, {})) {
        std::printf("xt_vc2carrier: the rate setter faults: %s\n", fw.cpu.error().c_str());
        return 1;
    }
    std::mt19937 rng((uint32_t)run->seed);
    Vc2Carrier c;
    Counts n;
    Vc2Tally tally { "XT_VC2CARRIER_FIRST", "XT_VC2CARRIER_STOP" };
    for (int k = 0; k < run->cases; ++k) {
        std::string what = initOn(fw, c, rng, clock);
        for (int b = 0; b < kBlocks && what.empty(); ++b) {
            if (rng() % kEventOneIn == 0) what = paramsEventOn(fw, c, rng, n);
            if (what.empty()) what = renderOn(fw, c, n);
        }
        if (vc2CaseOutcome(tally, what, k, n.blocks)) break;
    }
    std::printf("carrier: %d cases, %ld blocks (%ld growling), %ld updates (%ld with the Dynamics), %ld settings, %ld "
                "with no P, %ld different\n", run->cases, n.blocks, n.growling, n.updates, n.dynamics, n.settings,
                n.none, tally.different);
    const long osc = vc2VocoderOscCases(fw, *run, clock);   // the second part (v222)
    std::printf("xt_vc2carrier: %ld different\n", tally.different + osc);
    return 0;
}
#endif
