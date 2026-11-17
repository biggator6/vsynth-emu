// xt_vc2osc's ramp part (v236): Vc2PitchRamp against rom.4's own routines on three ramps in the interpreter's RAM, run
// before the voice part installs its hooks; logf and exp2 rom.4's own on both sides (RomShared).  Each case the ramps
// drawn the same on both sides and, three cases in four, built; then events on any of them: built, started from a block
// of drawn settings, restarted (by the restart and by the settings' routine, which is it), given a legato update,
// ticked, asked its value, or drawn again.  After each, the three ramps word for word and the answer.  Not drawn: a
// negative count or phase (the tone gives none; past the factor's table rom.4 reads what lies around it), a ratio's ramp
// given an end at or under 0 (logf's −∞ or NaN reaches exp2, which faults on a NaN).  XT_VC2OSC_MAIN only.
#ifdef XT_VC2OSC_MAIN
#include "Vc2OscSides.h"
#include "Vc2PitchRamp.h"
#include <bit>
#include <cmath>

namespace {
// [FW] the ramp's routines
constexpr uint32_t kBuild = 0x893b0dc0, kStart = 0x893b0e10, kRestart = 0x893b0b10, kSettings = 0x893b0f20;
constexpr uint32_t kLegato = 0x893b0e30, kTick = 0x893b0f30, kValue = 0x893b1160;
constexpr uint32_t kRampsAt = 0x8C050000, kRampStride = 0x100, kSettingsAt = 0x8C051000;   // in Vc2Fw's RAM
constexpr int kRamps = 3, kSteps = 300;
constexpr std::array<int32_t, kRamps> kBuiltTypes { 0, 2, 1 };   // [FW] as a voice's constructor builds them
constexpr float kPhaseEnd = 65536.0f;

struct RampCounts {
    long events = 0, built = 0, started = 0, restarted = 0, legatos = 0, ticks = 0, gliding = 0, ended = 0, values = 0;
};

struct RampRig {
    Vc2Fw& fw;
    const RomShared& shared;
    std::array<Vc2PitchRamp, kRamps> ramps {};
    std::array<uint32_t, sizeof(Vc2RampSettings) / 4> settings {};
    RampCounts n {};
};

uint32_t rampAt(int ramp) { return kRampsAt + kRampStride * (uint32_t)ramp; }

// A switch on half the time, else off or any word
uint32_t drawOnSwitch(std::mt19937& rng) { return rng() % 2 ? 1u : (rng() % 2 ? 0u : (uint32_t)rng()); }

// A glide's time: its law's edges (1, 50 and 100, either side of them, 0, negative, huge) or 1…3000 ms
float drawGlideTime(std::mt19937& rng) {
    constexpr std::array<float, 8> kEdges { 1.0f, 50.0f, 100.0f, 0.0f, 0.5f, 2.0f, -3.0f, 1e6f };
    const float edge = kEdges[rng() % kEdges.size()];
    if (rng() % 4 == 0) return edge;
    if (rng() % 6 == 0) return std::nextafter(edge, rng() % 2 ? -1e9f : 1e9f);
    return 1.0f + 3000.0f * vc2Unit(rng);
}

} // namespace

Vc2RampSettings oscDrawRampSettings(std::mt19937& rng) {
    Vc2RampSettings s;
    s.on = drawOnSwitch(rng);
    s.formantOn = drawOnSwitch(rng);
    s.walkOn = drawOnSwitch(rng);
    s.time = drawGlideTime(rng);
    s.timed = drawOnSwitch(rng);
    s.unread = (uint32_t)rng();
    return s;
}

namespace {
// A glide's ends in cents: from within ±6000; the span 0, about ±500 or ±1200, or any to ±20000
std::pair<float, float> drawCents(std::mt19937& rng) {
    constexpr std::array<float, 7> kSpans { 0.0f, 500.0f, -500.0f, 499.99f, 500.01f, 1200.0f, -1200.0f };
    const float from = 12000.0f * vc2Unit(rng) - 6000.0f;
    const float span = rng() % 2 ? kSpans[rng() % kSpans.size()] : 40000.0f * vc2Unit(rng) - 20000.0f;
    return { from, from + span };
}

// The ends a start or a restart is given by the ramp's type: cents, or for a ratio's ramp the same as ratios (1 now and
// then, whose logf is 0)
std::pair<float, float> drawEnds(const Vc2PitchRamp& r, std::mt19937& rng) {
    const auto [from, to] = drawCents(rng);
    if (r.w.type == 0) return { from, to };
    const float a = rng() % 8 == 0 ? 1.0f : std::exp2(from / 1200.0f);
    return { a, rng() % 8 == 0 ? a : std::exp2(to / 1200.0f) };
}

// A phase or a step: about 65536, 0, or any to 70000
float drawPhase(std::mt19937& rng) {
    constexpr std::array<float, 4> kEdges { kPhaseEnd, 0.0f, 32768.0f, 6553.6f };
    if (rng() % 3 == 0) return kEdges[rng() % kEdges.size()];
    if (rng() % 4 == 0) return std::nextafter(kPhaseEnd, rng() % 2 ? 0.0f : 1e9f);
    return 70000.0f * vc2Unit(rng);
}

// A ramp's words as a glide may leave them, each word in its range
void drawRamp(Vc2PitchRamp& r, std::mt19937& rng) {
    Vc2RampWords& w = r.w;
    w.type = rng() % 8 == 0 ? (int32_t)rng() : (int32_t)(rng() % 3);
    w.on = drawOnSwitch(rng);
    w.rising = (uint32_t)rng();
    w.settings = oscDrawRampSettings(rng);
    std::tie(w.from, w.to) = drawCents(rng);
    std::tie(w.fromGiven, w.toGiven) = drawCents(rng);
    w.state = rng() % 8 == 0 ? (int32_t)rng() : (int32_t)(rng() % 4);
    w.span = rng() % 2 ? xtf::sub(w.to, w.from) : oscDrawFloat(rng, OscRange::moderate);
    w.value = oscDrawFloat(rng, OscRange::moderate);
    w.step = drawPhase(rng);
    w.phase = drawPhase(rng);
    w.ease = rng() % 2 ? (float)(rng() % 2) : vc2Unit(rng);
    w.factorStep = 5000.0f + 5000.0f * vc2Unit(rng);
    w.factorPhase = drawPhase(rng);
    w.factor = oscDrawFloat(rng, OscRange::moderate);
    w.factorDepth = rng() % 2 ? 1.0f : 2.0f * vc2Unit(rng);
    w.factorRise = rng() % 2 ? 0.25f : oscDrawFloat(rng, OscRange::moderate);
    w.factorFall = rng() % 2 ? 0.5f : oscDrawFloat(rng, OscRange::moderate);
}

const char* const kNames[kRamps] = { "ramp 0", "ramp 1", "ramp 2" };

std::string rampsDifference(RampRig& s) {
    if (s.shared.faulted) return "rom.4's exp2 or logf faults for the port";
    for (int r = 0; r < kRamps; ++r) {
        const Vc2RampWords& w = s.ramps[(size_t)r].w;
        std::string what = vc2FirstDifference(s.fw, rampAt(r), &w, sizeof w, kNames[r]);
        if (!what.empty()) return what;
    }
    return "";
}

// rom.4's routine on `args` (fr4 and fr5 as given), then the port's `mine()`, then what differs
template <class Mine>
std::string bothRamps(RampRig& s, uint32_t routine, std::initializer_list<uint32_t> args, Mine mine,
                      std::pair<float, float> fr = {}) {
    s.fw.cpu.fb[0][4] = std::bit_cast<uint32_t>(fr.first);
    s.fw.cpu.fb[0][5] = std::bit_cast<uint32_t>(fr.second);
    if (!s.fw.call(routine, args)) return "rom.4's ramp faults: " + s.fw.cpu.error();
    mine();
    return rampsDifference(s);
}

std::string buildEvent(RampRig& s, std::mt19937& rng, int r) {
    const int32_t type = rng() % 4 ? kBuiltTypes[(size_t)r] : (int32_t)(rng() % 4);
    ++s.n.built;
    uint32_t r0 = 0;
    if (!s.fw.call(kBuild, { rampAt(r), (uint32_t)type }, &r0)) {
        return "rom.4's constructor faults: " + s.fw.cpu.error();
    }
    s.ramps[(size_t)r].construct(type);
    std::string what = rampsDifference(s);
    return what.empty() && r0 != rampAt(r) ? "the constructor's answer" : what;
}

void writeSettings(RampRig& s, std::mt19937& rng) {
    const Vc2RampSettings v = oscDrawRampSettings(rng);
    std::memcpy(s.settings.data(), &v, sizeof v);
    s.fw.cpu.write(kSettingsAt, s.settings.data(), sizeof s.settings);
}

std::string startEvent(RampRig& s, std::mt19937& rng, int r) {
    writeSettings(s, rng);
    const auto ends = drawEnds(s.ramps[(size_t)r], rng);
    ++s.n.started;
    return bothRamps(s, kStart, { rampAt(r), kSettingsAt }, [&] {
        s.ramps[(size_t)r].start(s.settings.data(), ends.first, ends.second, s.shared);
    }, ends);
}

std::string restartEvent(RampRig& s, std::mt19937& rng, int r) {
    const auto ends = drawEnds(s.ramps[(size_t)r], rng);
    ++s.n.restarted;
    return bothRamps(s, rng() % 2 ? kRestart : kSettings, { rampAt(r) }, [&] {
        s.ramps[(size_t)r].restart(ends.first, ends.second, s.shared);
    }, ends);
}

std::string legatoEvent(RampRig& s, std::mt19937& rng, int r) {
    writeSettings(s, rng);
    ++s.n.legatos;
    return bothRamps(s, kLegato, { rampAt(r), kSettingsAt }, [&] { s.ramps[(size_t)r].legato(s.settings.data()); });
}

// A count: a few blocks most often, now and then to 32767 and with high bits, which the tick drops (never negative)
uint32_t drawCount(std::mt19937& rng) {
    const uint32_t low = rng() % 3 ? rng() % 8 : rng() % 0x8000;
    return rng() % 4 == 0 ? (((uint32_t)rng() << 16) | low) : low;
}

std::string tickEvent(RampRig& s, std::mt19937& rng, int r) {
    const uint32_t n = drawCount(rng);
    Vc2PitchRamp& ramp = s.ramps[(size_t)r];
    ++s.n.ticks;
    s.n.gliding += ramp.w.on != 0 && ramp.w.state == 2;
    std::string what = bothRamps(s, kTick, { rampAt(r), n }, [&] { ramp.tick((int32_t)n, s.shared); });
    s.n.ended += ramp.w.on != 0 && ramp.w.state == 3;
    return what;
}

std::string valueEvent(RampRig& s, std::mt19937&, int r) {
    ++s.n.values;
    if (!s.fw.call(kValue, { rampAt(r) })) return "rom.4's value faults: " + s.fw.cpu.error();
    const float mine = s.ramps[(size_t)r].value();
    if (vc2SameAsFr0(s.fw, mine)) return rampsDifference(s);
    char t[96];
    std::snprintf(t, sizeof t, "%s's value: %08x, against %08x", kNames[r], std::bit_cast<uint32_t>(mine),
                  s.fw.cpu.fb[0][0]);
    return t;
}

std::string drawEvent(RampRig& s, std::mt19937& rng, int r) {
    drawRamp(s.ramps[(size_t)r], rng);
    s.fw.cpu.write(rampAt(r), &s.ramps[(size_t)r].w, sizeof(Vc2RampWords));
    return "";
}

using RampEvent = std::string (*)(RampRig&, std::mt19937&, int);
constexpr RampEvent kEvents[] = { buildEvent, startEvent, startEvent, restartEvent, legatoEvent, tickEvent, tickEvent,
                                  tickEvent, tickEvent, tickEvent, valueEvent, valueEvent, drawEvent };

std::string rampPartCase(RampRig& s, std::mt19937& rng) {
    const bool built = rng() % 4 != 0;
    for (int r = 0; r < kRamps; ++r) {
        drawEvent(s, rng, r);
        if (built) {
            std::string what = buildEvent(s, rng, r);
            if (!what.empty()) return what;
        }
    }
    std::string what;
    for (int k = 0; k < kSteps && what.empty(); ++k, ++s.n.events) {
        what = kEvents[rng() % std::size(kEvents)](s, rng, (int)(rng() % kRamps));
    }
    return what;
}
} // namespace

void oscDrawBuiltRamps(std::array<Vc2PitchRamp, Vc2OscVoice::kRamps>& ramps, std::mt19937& rng) {
    for (size_t r = 0; r < ramps.size(); ++r) {
        drawRamp(ramps[r], rng);
        ramps[r].w.type = kBuiltTypes[r];
    }
}

void oscRampPart(Vc2Fw& fw, const RomShared& shared, const Vc2Run& run, Vc2Tally& tally) {
    RampRig s { fw, shared };
    vc2RunCases(run, tally, "", s.n.events, [&](std::mt19937& rng, bool) { return rampPartCase(s, rng); });
    const RampCounts& n = s.n;
    std::printf("ramp: %d cases, %ld events: %ld built, %ld started, %ld restarted, %ld legato updates, %ld ticks "
                "(%ld gliding, %ld at an end), %ld values\n", run.cases, n.events, n.built, n.started, n.restarted,
                n.legatos, n.ticks, n.gliding, n.ended, n.values);
}
#endif
