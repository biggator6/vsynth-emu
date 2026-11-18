// xt_vc2osc's vibrato part (v267): Vc2OscVibrato against rom.4's own routines on an object in the interpreter's RAM,
// before the voice part hooks anything, after the static initializer 0x893b2d60.  Each case the object drawn, built and
// given a note-on, then 300 events: an update (most of them), the settings, another note-on, a build, or the words drawn
// again; the object word for word after each.  And the draws the voice part shares: the words as a note leaves them, a
// block as the converter could write it (waves 0 and 1 alone: past them rom.4 reads what lies after its table).  The
// fade's rates are held to 1 over the count, as past that the fade runs away to infinities whose NaNs are the host's.
// XT_VC2OSC_MAIN only.
#ifdef XT_VC2OSC_MAIN
#include "Vc2OscSides.h"
#include <algorithm>
#include <bit>
#include <cstring>

namespace {
constexpr uint32_t kStatic = 0x893b2d60, kBuild = 0x893b29b0, kNoteOn = 0x893b29e0;   // [FW]
constexpr uint32_t kSettings = 0x893b2b70, kUpdate = 0x893b2c10;                      // [FW]
constexpr uint32_t kObjectAt = 0x8C05fa00, kBlockAt = 0x8C05fb00;   // in Vc2Fw's RAM
constexpr int kSteps = 300;
constexpr uint32_t kBlockWords = sizeof(Vc2VibratoBlock) / 4;
constexpr uint32_t kBlockFirst = 0x80 / 4;   // [FW] the block's word the vibrato's start at, in a block or the settings
constexpr uint32_t kVoiceBlock = 0xd4;       // [FW] the same in a voice

struct VibratoRig {
    Vc2Fw& fw;
    Vc2OscVibrato vibrato {};
    long events = 0, updates = 0, falling = 0, sounding = 0;
};

// A delay, rise, hold or fall of the block's: none, its edges, or up to two seconds' worth
float drawVibratoSpan(std::mt19937& rng) {
    constexpr std::array<float, 4> kEdges { 0.0f, -0.0f, 1.0f, 2000.0f };
    return rng() % 3 == 0 ? kEdges[rng() % kEdges.size()] : oscWithin(rng, 1.0f, 2000.0f);
}

// Vibrato Rate: the converter's 2…8 most often, and past both ends of the step's bounds
float drawVibratoRate(std::mt19937& rng) {
    constexpr std::array<float, 4> kEdges { 0.0f, -1.0f, 300.0f, 0.001f };
    return rng() % 4 == 0 ? kEdges[rng() % kEdges.size()] : oscWithin(rng, 2.0f, 8.0f);
}

// The block's +0x24: the converter's 0…63, and 100 with either side of it
int32_t drawVibratoScaleBy(std::mt19937& rng) {
    constexpr std::array<int32_t, 4> kEdges { 99, 100, 101, 200 };
    return rng() % 3 == 0 ? kEdges[rng() % kEdges.size()] : (int32_t)(rng() % 64);
}

Vc2VibratoBlock drawVibratoBlock(std::mt19937& rng) {
    Vc2VibratoBlock b {};
    b.wave = (int32_t)(rng() % kVc2VibratoWaves);
    b.rate = drawVibratoRate(rng);
    b.pitchBias = oscWithin(rng, -1.0f, 1.0f);
    b.levelBias = oscWithin(rng, -1.0f, 1.0f);
    b.delay = rng() % 4 == 0 ? -drawVibratoSpan(rng) : drawVibratoSpan(rng);
    b.hold = drawVibratoSpan(rng);
    b.rise = rng() % 4 == 0 ? -oscWithin(rng, 0.0f, b.hold) : drawVibratoSpan(rng);   // never a sum below 0, as the rows'
    b.fall = rng() % 4 == 0 ? -drawVibratoSpan(rng) : drawVibratoSpan(rng);
    b.stepRatio = rng() % 2 ? 1.0f : oscWithin(rng, 0.0f, 1.5f);
    b.scaleBy = drawVibratoScaleBy(rng);
    return b;
}

// The fade's rates held so that a count of `n` can't overshoot its target
void holdVibratoRates(Vc2VibratoWords& w, int32_t n) {
    const float most = 1.0f / (float)std::max(n, 1);
    w.fadeRate = std::min(w.fadeRate, most);
    w.fallRate = std::min(w.fallRate, 0.1f * most);
}

std::string vibratoDifference(VibratoRig& r) {
    return vc2FirstDifference(r.fw, kObjectAt, &r.vibrato.w, sizeof r.vibrato.w, "the vibrato's words");
}

std::string vibratoBuildEvent(VibratoRig& r) {
    return vc2BothOn(r.fw, kBuild, { kObjectAt }, "the build", [&] { r.vibrato.build(); },
                     [&] { return vibratoDifference(r); });
}

// A routine given a drawn block: the note-on or the settings
using VibratoBlockRoutine = void (Vc2OscVibrato::*)(const Vc2VibratoBlock&);
std::string vibratoBlockEvent(VibratoRig& r, std::mt19937& rng, uint32_t routine, const char* name,
                              VibratoBlockRoutine port) {
    const Vc2VibratoBlock b = drawVibratoBlock(rng);
    r.fw.cpu.write(kBlockAt, &b, sizeof b);
    return vc2BothOn(r.fw, routine, { kObjectAt, kBlockAt }, name, [&] { (r.vibrato.*port)(b); },
                     [&] { return vibratoDifference(r); });
}

std::string vibratoNoteOnEvent(VibratoRig& r, std::mt19937& rng) {
    return vibratoBlockEvent(r, rng, kNoteOn, "the note-on", &Vc2OscVibrato::noteOn);
}

std::string vibratoUpdateEvent(VibratoRig& r, std::mt19937& rng) {
    const int32_t n = rng() % 8 == 0 ? (int32_t)(rng() % 2000) : (int32_t)(rng() % 64);
    holdVibratoRates(r.vibrato.w, n);
    r.fw.cpu.write(kObjectAt, &r.vibrato.w, sizeof r.vibrato.w);
    ++r.updates;
    std::string what = vc2BothOn(r.fw, kUpdate, { kObjectAt, (uint32_t)n }, "the update", [&] { r.vibrato.update(n); },
                                 [&] { return vibratoDifference(r); });
    r.falling += r.vibrato.w.falling != 0;
    r.sounding += r.vibrato.w.pitch != 0.0f;
    return what;
}

void vibratoDrawnAgain(VibratoRig& r, std::mt19937& rng) {
    oscDrawVibratoWords(r.vibrato.w, rng);
    r.fw.cpu.write(kObjectAt, &r.vibrato.w, sizeof r.vibrato.w);
}

std::string vibratoEvent(VibratoRig& r, std::mt19937& rng) {
    const uint32_t pick = rng() % 16;
    if (pick < 11) return vibratoUpdateEvent(r, rng);
    if (pick < 13) return vibratoBlockEvent(r, rng, kSettings, "the settings", &Vc2OscVibrato::settings);
    if (pick < 14) return vibratoNoteOnEvent(r, rng);
    if (pick < 15) return vibratoBuildEvent(r);
    vibratoDrawnAgain(r, rng);
    return "";
}

std::string vibratoCase(VibratoRig& r, std::mt19937& rng) {
    vibratoDrawnAgain(r, rng);
    std::string what = vibratoBuildEvent(r);
    if (what.empty()) what = vibratoNoteOnEvent(r, rng);
    for (int k = 0; k < kSteps && what.empty(); ++k, ++r.events) what = vibratoEvent(r, rng);
    return what;
}
} // namespace

void oscDrawVibratoWords(Vc2VibratoWords& w, std::mt19937& rng) {
    w.wave = (int32_t)(rng() % kVc2VibratoWaves);
    w.waveAt = kVc2VibratoWavesAt + (uint32_t)w.wave * kVc2VibratoWaveBytes;
    w.rate = oscWithin(rng, 2.0f, 8.0f);
    w.pitchBias = oscWithin(rng, -1.0f, 1.0f);
    w.levelBias = oscWithin(rng, -1.0f, 1.0f);
    w.riseHold = rng() % 4 == 0 ? 0.0f : oscWithin(rng, 0.0f, 3000.0f);
    w.time = oscWithin(rng, -2000.0f, 3000.0f);
    w.fadeTarget = (float)(rng() % 2);
    w.fade = vc2Unit(rng);
    w.fadeRate = oscWithin(rng, 0.0f, 0.02f);
    w.fallRate = oscWithin(rng, 0.0f, 0.002f);
    w.stepRatio = oscWithin(rng, 0.0f, 1.5f);
    w.pitch = oscWithin(rng, -2.0f, 2.0f);
    w.level = oscWithin(rng, -2.0f, 2.0f);
    w.active = rng() % 4 != 0;
    w.falling = rng() % 2;
    w.phase = rng() % 8 == 0 ? oscWithin(rng, -70000.0f, 0.0f) : oscWithin(rng, 0.0f, 70000.0f);
    w.step = oscWithin(rng, 0.0f, 4000.0f);
    w.stepTarget = oscWithin(rng, 1.0f, 4000.0f);
    w.scale = oscWithin(rng, 0.8f, 1.7f);
    w.glide = w.scale * w.scale * 0.07f;
}

void oscDrawVibratoBlock(std::span<uint32_t> words, std::mt19937& rng) {
    const Vc2VibratoBlock b = drawVibratoBlock(rng);
    std::memcpy(words.data() + kBlockFirst, &b, sizeof b);
}

void oscDrawVibratoVoice(Vc2OscVoice& o, std::mt19937& rng) {
    const auto words = std::bit_cast<std::array<uint32_t, kBlockWords>>(drawVibratoBlock(rng));
    for (uint32_t k = 0; k < kBlockWords; ++k) o.setU32(kVoiceBlock + 4 * k, words[k]);
}

void oscHoldVibrato(Vc2VibratoWords& w, int32_t n) { holdVibratoRates(w, n); }

void oscVibratoPart(Vc2Fw& fw, const Vc2Run& run, Vc2Tally& tally) {
    VibratoRig r { fw };
    const bool ready = fw.call(kStatic, {});
    const std::string atStart = ready ? std::string() : "the static initializer faults: " + fw.cpu.error();
    vc2RunCases(run, tally, atStart, r.events, [&](std::mt19937& rng, bool) { return vibratoCase(r, rng); });
    std::printf("vibrato: %d cases, %ld events, %ld updates (%ld falling, %ld with a pitch)\n", run.cases, r.events,
                r.updates, r.falling, r.sounding);
}
#endif
