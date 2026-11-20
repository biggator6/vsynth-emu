// xt_vc2osc's vibrato part (v267): Vc2OscVibrato against rom.4's own routines on an object in the interpreter's RAM,
// before the voice part hooks anything, after the static initializer 0x893b2d60.  Each case the object drawn, built and
// given a note-on, then 300 events: an update (most of them), the settings, another note-on, a build, or the words drawn
// again; the object word for word after each.  And the draw the voice part shares: the words as a note leaves them.  A
// block as the converter could write it and the fade's rates held for a count are Vc2OscDraws's from v319, shared with
// xt_vc2tone.  XT_VC2OSC_MAIN only.
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

struct VibratoRig {
    Vc2Fw& fw;
    Vc2OscVibrato vibrato {};
    long events = 0, updates = 0, falling = 0, sounding = 0;
};

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
    const Vc2VibratoBlock b = oscVibratoBlockDrawn(rng);
    r.fw.cpu.write(kBlockAt, &b, sizeof b);
    return vc2BothOn(r.fw, routine, { kObjectAt, kBlockAt }, name, [&] { (r.vibrato.*port)(b); },
                     [&] { return vibratoDifference(r); });
}

std::string vibratoNoteOnEvent(VibratoRig& r, std::mt19937& rng) {
    return vibratoBlockEvent(r, rng, kNoteOn, "the note-on", &Vc2OscVibrato::noteOn);
}

std::string vibratoUpdateEvent(VibratoRig& r, std::mt19937& rng) {
    const int32_t n = rng() % 8 == 0 ? (int32_t)(rng() % 2000) : (int32_t)(rng() % 64);
    oscHoldVibrato(r.vibrato.w, n);
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

void oscVibratoPart(Vc2Fw& fw, const Vc2Run& run, Vc2Tally& tally) {
    VibratoRig r { fw };
    const bool ready = fw.call(kStatic, {});
    const std::string atStart = ready ? std::string() : "the static initializer faults: " + fw.cpu.error();
    vc2RunCases(run, tally, atStart, r.events, [&](std::mt19937& rng, bool) { return vibratoCase(r, rng); });
    std::printf("vibrato: %d cases, %ld events, %ld updates (%ld falling, %ld with a pitch)\n", run.cases, r.events,
                r.updates, r.falling, r.sounding);
}
#endif
