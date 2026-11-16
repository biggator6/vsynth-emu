// xt_vc2kind6 <rom.4> [cases] [seed] (v225): the VC-2's voice kind 6 and the mic ring, Vc2Kind6 and Vc2MicRing,
// against rom.4's own routines on voices in the interpreter's RAM: the globals' setter (0x893ecef0) once, then each case
// the voices cleared, now and then the mode setter (0x893ec890, −1…2) before the ring's reset (0x893ec7d0) and, one case
// in three, mode 1 after it (the interval, which nothing in rom.4 selects), Unvoice Level drawn (0x899d9968), and
// blocks of the mic (Vc2TrackerSignal) through the ring's write (0x893ec820), the tracker's period drawn into
// 0x899d9bd0 (the tone's, an octave off, or held), then every started voice's render (0x893ec730) on a drawn unvoiced
// decision (0x899d99dc, the input stage's, which 0x893ed500 points at); one block in 20 an event first: a voice's set
// and start (0x893ec700, a drawn P), its update (0x893ec790, P or none), the mode setter or the ring's reset.  After each
// routine the ring, the globals and the mode word word for word, and the voice and its output.
// XT_VC2KIND6_FIRST=1 says where the first case to differ does, XT_VC2KIND6_STOP=1 stops there.  XT_VC2KIND6_MAIN only.
#ifdef XT_VC2KIND6_MAIN
#include "Vc2CheckRig.h"
#include "Vc2Kind6.h"
#include "Vc2TrackerSignal.h"
#include <cmath>
#include <cstdio>

namespace {
constexpr uint32_t kGlobalsSetter = 0x893ecef0, kReset = 0x893ec7d0, kWrite = 0x893ec820, kMode = 0x893ec890; // [FW]
constexpr uint32_t kSetStart = 0x893ec700, kSet = 0x893ec790, kRender = 0x893ec730;                         // [FW]
constexpr uint32_t kRingAt = 0x8a0f31cc, kGlobalsAt = 0x899d9968, kModeAt = 0x8993fe54;                     // [FW]
constexpr uint32_t kPeriodAt = 0x899d9bd0, kUnvoicedAt = 0x899d99dc;                                        // [FW]
constexpr uint32_t kInputAt = 0x8C035000, kOutAt = 0x8C036000, kParamsAt = 0x8C037000;   // in Vc2Fw's RAM
constexpr uint32_t kVoicesAt = 0x8C040000, kVoiceStride = 0x100;
constexpr int kVoices = 3, kBlocks = 800, kEventOneIn = 20, kFirstStart = 16;
constexpr int kFrames = 2 * kVc2Block;   // a render's two words a frame

struct Counts {
    long blocks = 0, renders = 0, interval = 0, starts = 0, updates = 0, modes = 0, resets = 0;
};

// Both sides: rom.4 in its interpreter, the port's ring and voices, which have started this case, what was counted
struct Sides {
    Vc2Fw& fw;
    Vc2MicRing& ring;
    std::array<Vc2Kind6, kVoices>& voices;
    std::array<bool, kVoices> started {};
    Counts n;
};

uint32_t voiceAt(int voice) { return kVoicesAt + kVoiceStride * (uint32_t)voice; }

std::string ringDifference(Sides& b) {
    std::string what = vc2FirstDifference(b.fw, kRingAt, &b.ring.w, sizeof b.ring.w, "the mic ring");
    if (what.empty()) what = vc2FirstDifference(b.fw, kGlobalsAt, &b.ring.g, sizeof b.ring.g, "the globals");
    if (what.empty()) what = vc2FirstDifference(b.fw, kModeAt, &b.ring.mode, sizeof b.ring.mode, "the mode word");
    return what;
}

std::string voiceDifference(Sides& b, int voice) {
    std::string what = vc2FirstDifference(b.fw, voiceAt(voice), &b.voices[(size_t)voice].w, sizeof(Vc2Kind6Words),
                                          "a voice's words");
    return what.empty() ? ringDifference(b) : what;
}

// P as a note gives it: the pitch over 440 Hz from 44 Hz to 2.2 kHz (the interval the same, in mode 1), the read rate
// 0.5…2, the gain and the level as 0…127 give them
Vc2Kind6Params drawParams(std::mt19937& rng) {
    return { 0.1f * std::pow(50.0f, vc2Unit(rng)), 0.5f * std::pow(4.0f, vc2Unit(rng)), vc2DrawLevel(rng),
             vc2DrawLevel(rng) };
}

std::string voiceStartOn(Sides& b, int voice, const Vc2Kind6Params& p) {
    ++b.n.starts;
    b.started[(size_t)voice] = true;
    b.fw.cpu.write(kParamsAt, &p, sizeof p);
    return vc2BothOn(
        b.fw, kSetStart, { voiceAt(voice), kParamsAt }, "the set and start",
        [&] { b.voices[(size_t)voice].setAndStart(b.ring, p); },
        [&] { return voiceDifference(b, voice); });
}

std::string voiceUpdateOn(Sides& b, int voice, const Vc2Kind6Params& p, bool none) {
    ++b.n.updates;
    b.fw.cpu.write(kParamsAt, &p, sizeof p);
    return vc2BothOn(
        b.fw, kSet, { voiceAt(voice), none ? 0u : kParamsAt }, "the update",
        [&] { b.voices[(size_t)voice].set(none ? nullptr : &p); },
        [&] { return voiceDifference(b, voice); });
}

std::string ringModeOn(Sides& b, int32_t mode) {
    ++b.n.modes;
    return vc2BothOn(
        b.fw, kMode, { (uint32_t)mode }, "the mode setter",
        [&] { b.ring.setMode(mode); },
        [&] { return ringDifference(b); });
}

std::string ringResetOn(Sides& b) {
    return vc2BothOn(
        b.fw, kReset, {}, "the ring's reset",
        [&] { b.ring.reset(); },
        [&] { return ringDifference(b); });
}

// An event between blocks: a voice's start or update (P or none), the mode setter, or the ring's reset
std::string kind6EventOn(Sides& b, std::mt19937& rng) {
    const int voice = (int)(rng() % kVoices);
    const uint32_t pick = rng() % 20;
    if (pick < 8) return voiceStartOn(b, voice, drawParams(rng));
    if (pick < 16) return voiceUpdateOn(b, voice, drawParams(rng), pick >= 14);
    if (pick < 19) return ringModeOn(b, (int32_t)(rng() % 4) - 1);
    ++b.n.resets;
    return ringResetOn(b);
}

// The input stage's decision a sample: voiced (0–3), unvoiced (4), a ramp between two draws (5, 6), or any (7)
void drawUnvoiced(std::mt19937& rng, float* u) {
    const uint32_t shape = rng() % 8;
    float from = vc2Unit(rng), to = vc2Unit(rng);
    if (shape < 4) from = to = 0.0f;
    if (shape == 4) from = to = 1.0f;
    for (int i = 0; i < kVc2Block; ++i) {
        u[i] = shape == 7 ? vc2Unit(rng) : from + (to - from) * (float)i / kVc2Block;
    }
}

// One started voice's render on both, its output and its words after
std::string kind6RenderOn(Sides& b, int voice, const float* u) {
    ++b.n.renders;
    b.n.interval += b.ring.w.pitchMode == 0 ? 1 : 0;
    float out[kFrames] = {};
    b.fw.cpu.write(kOutAt, out, sizeof out);
    std::string what = vc2BothOn(
        b.fw, kRender, { voiceAt(voice), kOutAt }, "the render",
        [&] { b.voices[(size_t)voice].render(b.ring, u, out); },
        [&] { return voiceDifference(b, voice); });
    if (what.empty()) what = vc2FirstDifference(b.fw, kOutAt, out, sizeof out, "the output");
    return what;
}

// A block of the mic into the ring with the tracker's period, then every started voice
std::string kind6BlockOn(Sides& b, const float* x, float period, std::mt19937& rng) {
    ++b.n.blocks;
    b.fw.cpu.write(kInputAt, x, sizeof(float) * kVc2Block);
    b.fw.cpu.write(kPeriodAt, &period, sizeof period);
    std::string what = vc2BothOn(
        b.fw, kWrite, { kInputAt, (uint32_t)kVc2Block }, "the ring's write",
        [&] { b.ring.write(x, period); },
        [&] { return ringDifference(b); });
    float u[kVc2Block];
    drawUnvoiced(rng, u);
    b.fw.cpu.write(kUnvoicedAt, u, sizeof u);
    for (int voice = 0; voice < kVoices && what.empty(); ++voice) {
        if (b.started[(size_t)voice]) what = kind6RenderOn(b, voice, u);
    }
    return what;
}

// The tracker's period for a block: the tone's, now and then an octave off as the tracker errs, else held
float trackedPeriod(std::mt19937& rng, float tone, float last) {
    constexpr float kOctaves[] = { 1.0f, 1.0f, 1.0f, 2.0f, 0.5f };
    if (rng() % 64 == 0) return 20.0f * std::pow(50.0f, vc2Unit(rng));
    if (tone > 0.0f && rng() % 8 == 0) return tone * kOctaves[rng() % std::size(kOctaves)];
    return last;
}

// The case: the voices cleared, the ring reset (the mode set around it now and then), then its blocks
std::string kind6Case(Sides& b, std::mt19937& rng, bool fades) {
    const std::array<uint8_t, kVoiceStride> zeros {};
    for (int voice = 0; voice < kVoices; ++voice) {
        b.fw.cpu.write(voiceAt(voice), zeros.data(), zeros.size());
        b.voices[(size_t)voice].w = Vc2Kind6Words {};
        b.started[(size_t)voice] = false;
    }
    std::string what = rng() % 4 == 0 ? ringModeOn(b, (int32_t)(rng() % 4) - 1) : "";
    if (what.empty()) what = ringResetOn(b);
    if (what.empty() && rng() % 3 == 0) what = ringModeOn(b, 1);
    b.ring.g.unvoiceLevel = vc2DrawLevel(rng);
    b.fw.cpu.write(kGlobalsAt, &b.ring.g.unvoiceLevel, sizeof(float));
    Vc2TrackerSignal input(rng(), fades);
    float x[kVc2Block];
    float period = b.ring.g.a440Period;
    for (int k = 0; k < kBlocks && what.empty(); ++k) {
        if (k == kFirstStart) what = voiceStartOn(b, 0, drawParams(rng));
        if (what.empty() && rng() % kEventOneIn == 0) what = kind6EventOn(b, rng);
        input.block(x);
        period = trackedPeriod(rng, input.tonePeriod(), period);
        if (what.empty()) what = kind6BlockOn(b, x, period, rng);
    }
    return what;
}
} // namespace

int main(int argc, char** argv) {
    Vc2Fw fw;
    const auto run = vc2Start(argc, argv, fw, "xt_vc2kind6", vc2LoadKind6);
    if (!run) return 1;
    Vc2MicRing ring;
    std::array<Vc2Kind6, kVoices> voices {};
    Sides b { fw, ring, voices, {}, {} };
    const std::string what = vc2BothOn(
        fw, kGlobalsSetter, {}, "the globals' setter",
        [&] { ring.setGlobals(); },
        [&] { return ringDifference(b); });
    Vc2Tally tally { "XT_VC2KIND6_FIRST", "XT_VC2KIND6_STOP" };
    vc2RunCases(*run, tally, what, b.n.blocks, [&](std::mt19937& rng, bool fades) { return kind6Case(b, rng, fades); });
    const Counts& n = b.n;
    std::printf("kind 6: %d cases, %ld blocks, %ld renders (%ld in mode 1), %ld starts, %ld updates, %ld modes, "
                "%ld resets, %ld different\n", run->cases, n.blocks, n.renders, n.interval, n.starts, n.updates,
                n.modes, n.resets, tally.different);
    std::printf("xt_vc2kind6: %ld different\n", tally.different);
    return 0;
}
#endif
