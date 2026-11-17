// xt_vc2osc <rom.4> [cases] [seed] (v230): the VC-2's OSC voice, Vc2OscVoice, against rom.4's own routines on two
// voices in the interpreter's RAM, every routine they call hooked to log and answer as drawn (Vc2OscFakes.cpp), the
// port's parts the same (Vc2OscPortParts.cpp); exp2 and the block-move helper rom.4's own on both sides.  Each case
// both voices filled with the same noise, then 400 events (Vc2OscEvents.cpp) on either voice: constructed, installed,
// updated (the count with high bits), re-sought, a note-on on a block drawn in the interpreter's RAM with its records,
// the ramp's start, a count or a matched count, the settings from a drawn block, the legato, a render into a buffer of
// noise, a take of the other voice's object, or a getter.  Before each, the hooked routines' answers, the records and,
// now and then, the words each routine decides by (the kind, the wave's type, the object's flags and level, the legato
// and gate words, the pending seek, the render routines, the first binding) drawn and written on both sides.  After
// each, both voices' own words (+0x54…+0x90c and +0xa38…+0xa90), the calls, the answer and the output.
// XT_VC2OSC_FIRST=1 says where the first case to differ does, XT_VC2OSC_STOP=1 stops there.  XT_VC2OSC_MAIN only.
#ifdef XT_VC2OSC_MAIN
#include "Vc2CheckCalls.h"
#include "Vc2OscSides.h"
#include "Vc2RampTables.h"
#include "Vc2OscTables.h"
#include <bit>
#include <cmath>

namespace {
constexpr int kSteps = 400;
// [FW] what the voice reads of its parts in place: the vibrato's two outputs
constexpr uint32_t kVibratoPitch = 0x2c, kVibratoLevel = 0x30;
constexpr uint32_t kRamps = 0x90c, kTail = 0xa38;   // [FW] where the ramps (v236) and the tail are in a voice
// [FW] a bound envelope's words the targets read (+0x1c, at 0 one time in two, +0x2c, +0x4c) and an LFO's (+0x38)
constexpr std::array<uint32_t, 3> kEnvelopeWords { 0x1c, 0x2c, 0x4c };
constexpr uint32_t kEnvelopeOut = 0x4c, kLfoOut = 0x38;
constexpr size_t kEnvelopeBinds = 4;
constexpr std::array<uint32_t, 4> kRenders { 0x893c62e0, 0x893c61e0, 0, 0 };   // [FW] the install's two, and two hooked
constexpr float kTwo31 = 2147483648.0f;
// [FW] the words each routine decides by: the kind, the wave's type, the block's byte +0x138, the object's flags, the
// source's byte +4, its seek word, level, +0x168, positions, the legato word, the take's gate; and the block's floats
// the pitch, the ramp's start and the targets read
constexpr uint32_t kKind = 0x54, kWave = 0x104, kFixed = 0x138, kFlags = 0x15c, kSourceByte = 0x160, kSeekAt = 0x1c0;
constexpr uint32_t kLevel = 0x1e0, kHalf = 0x168, kPosition = 0x194, kPositionLite = 0x24c, kPositionFloat = 0x270;
constexpr uint32_t kLegato = 0x908, kGate = 0xb8;
constexpr std::array<uint32_t, 8> kFloatWords { 0x60, 0x64, 0x6c, 0x78, 0x88, 0x98, 0xb0, 0x100 };
// [FW] those that reach exp2 through the targets: the pitch, the formant, the pitch envelope's depth, the formant
// envelope's and LFO's, the vibrato's on the pitch; and where the pitch ramp starts from (+0xbc, v236: the ramp's
// value is a term of the pitch) (each the block's word 0x54 below)
constexpr std::array<uint32_t, 7> kModerateWords { 0x5c, 0x68, 0x70, 0x80, 0xa0, 0xfc, 0xbc };
// The calls counted over the run, by kind
constexpr std::array<std::pair<OscKind, long OscCounts::*>, 2> kTallied { {
    { kSeek, &OscCounts::seeks }, { kKindCall, &OscCounts::kindCalls } } };

uint32_t drawKind(std::mt19937& rng) {   // 0…7, now and then past 7
    constexpr std::array<uint32_t, 4> kPast { 8, 9, 100, 0xffffffff };
    return rng() % 12 == 0 ? kPast[rng() % kPast.size()] : rng() % 8;
}

// The object's level around the update's wait: at it, a unit either side, 0, or any
float drawLevel(std::mt19937& rng) {
    const float wait = vc2OscTables().waitLevel;
    const std::array<float, 4> near { wait, std::nextafter(wait, INFINITY), std::nextafter(wait, 0.0f), 0.0f };
    return rng() % 2 ? near[rng() % near.size()] : oscDrawFloat(rng);
}

uint32_t maybeZero(std::mt19937& rng) { return rng() % 2 ? 0 : (uint32_t)rng(); }

// The words the routines decide by, each drawn one time in four (all of them at a case's start)
void steer(Vc2OscVoice& o, std::mt19937& rng, bool all) {
    const auto now = [&] { return all || rng() % 4 == 0; };
    if (now()) o.setU32(kKind, drawKind(rng));
    if (now()) o.setU32(kWave, rng() % 8 == 0 ? (uint32_t)rng() : rng() % 10);
    if (now()) o.setU8(kFixed, (uint8_t)rng());
    if (now()) o.setU8(kFlags, (uint8_t)rng());
    if (now()) o.setU8(kSourceByte, rng() % 3 == 0 ? 1 : (uint8_t)rng());
    if (now()) o.setU32(kSeekAt, (uint32_t)rng());
    if (now()) o.setF32(kLevel, drawLevel(rng));
    if (now()) o.setU32(kLegato, maybeZero(rng));
    if (now()) o.setU32(kGate, maybeZero(rng));
    for (uint32_t at : { kHalf, kPosition, kPositionLite }) {
        if (now()) o.setU32(at, (uint32_t)rng());
    }
    if (now()) o.setF32(kPositionFloat, oscDrawPosition(rng));
    for (uint32_t at : kFloatWords) {
        if (now()) o.setF32(at, oscDrawFloat(rng));
    }
    for (uint32_t at : kModerateWords) {
        if (now()) o.setF32(at, oscDrawFloat(rng, OscRange::moderate));
    }
    if (now()) o.tail.seekPending = rng() % 2 ? 1 : maybeZero(rng);
    for (float* x : { &o.tail.rateRatio, &o.tail.walkFactor, &o.tail.formantFactor }) {
        if (now()) *x = oscDrawFloat(rng);
    }
    // the offset and the detune reach exp2; and no NaN in the detune, which the pitch getter subtracts: xtf::sub's
    // a + (−b) gives a NaN's sign negated where the interpreter's FSUB keeps it (a NaN's sign alone)
    for (float* x : { &o.tail.offset, &o.tail.detune }) {
        if (now()) *x = oscDrawFloat(rng, OscRange::moderate);
    }
    if (now()) o.tail.sync = (uint32_t)rng();
    const uint32_t render = kRenders[rng() % kRenders.size()];
    if (now()) o.tail.render = render != 0 ? render : oscRenderAt((int)(rng() % kOscRenders));
    if (now()) o.tail.renderAnalog = oscRenderAt((int)(rng() % kOscRenders));
    for (size_t b = 0; b < o.tail.binds.size(); ++b) {   // four envelopes, then four LFOs: every one read somewhere
        const int k = (int)(rng() % kOscRecords);
        if (now()) o.tail.binds[b] = b < kEnvelopeBinds ? oscEnvelopeAt(k) : oscLfoAt(k);
    }
}

// A loop record's tune: within ±12 semitones most often, any word one time in four, and one time in eight about where
// the factor, 2^(−tune/120000), leaves the floats' range (2^∓126…2^∓151), so the note-on's test of it meets a 0
int32_t drawTune(std::mt19937& rng) {
    constexpr std::array<int32_t, 6> kOctaves { 126, 127, 128, 149, 150, 151 };
    constexpr int32_t kPerOctave = 120000;   // [FW] 0x893ae644's 1/120000
    if (rng() % 8 == 0) {
        const int32_t sign = rng() % 2 ? 1 : -1;
        return sign * kOctaves[rng() % kOctaves.size()] * kPerOctave + (int32_t)(rng() % 3) - 1;
    }
    return rng() % 4 == 0 ? (int32_t)rng() : (int32_t)(rng() % 2400) - 1200;
}

void drawRecords(OscRecords& r, std::mt19937& rng) {
    constexpr std::array<uint16_t, 5> kRates { 44100, 32000, 22050, 48000, 0 };
    for (int k = 0; k < kOscRecords; ++k) {
        Vc2OscWaveHeader& h = r.headers[(size_t)k];
        h.flag = rng() % 3 == 0 ? (uint8_t)rng() : (uint8_t)(rng() % 2);
        h.rate = rng() % 3 == 0 ? (uint16_t)rng() : kRates[rng() % kRates.size()];
        h.rootKey = (int8_t)rng();
        h.cents = rng() % 4 == 0 ? (int32_t)rng() : (int32_t)(rng() % 128);
        r.loops[(size_t)k] = { drawTune(rng), (int8_t)rng() };
        for (uint32_t word : kEnvelopeWords) r.bound[oscEnvelopeAt(k) + word] = oscDrawFloat(rng);
        if (rng() % 2) r.bound[oscEnvelopeAt(k) + kEnvelopeWords[0]] = 0.0f;
        // the outputs the pitch and the formant take, on their way to exp2
        r.bound[oscEnvelopeAt(k) + kEnvelopeOut] = oscDrawFloat(rng, OscRange::moderate);
        r.bound[oscLfoAt(k) + kLfoOut] = oscDrawFloat(rng, OscRange::moderate);
    }
}

// The draws before an event, written on both sides: the answers, the records, the steered words, each voice's own
// words and tail into rom.4's memory and its vibrato's output
void drawBefore(OscSides& s, std::mt19937& rng, bool all) {
    OscDraws& d = s.shared.draws;
    d.vibratoPitch = oscDrawFloat(rng, OscRange::moderate);   // into the pitch
    d.vibratoLevel = oscDrawFloat(rng);
    d.rendered = (uint32_t)rng();
    d.seekFlags = (uint8_t)rng();
    drawRecords(s.shared.records, rng);
    oscWriteRecords(s.fw, s.shared.records);
    for (int v = 0; v < kOscVoices; ++v) {
        steer(s.voices[(size_t)v], rng, all);
        oscWriteVoice(s, v);
        const std::array<std::pair<uint32_t, float>, 2> inPlace { { { kVibratoPitch, d.vibratoPitch },
                                                                     { kVibratoLevel, d.vibratoLevel } } };
        for (const auto& [at, x] : inPlace) s.fw.cpu.write(oscVoiceAt(v) + at, &x, sizeof x);
    }
    s.rom.clear();
    s.shared.calls.clear();
}

// A case: both voices the same noise, their ramps as glides leave them (each of its built type: the voice restarts the
// pitch's from its own words, and a ratio's ramp from those would take logf to a NaN), then its events
std::string oscVoiceCase(OscSides& s, std::mt19937& rng) {
    for (Vc2OscVoice& o : s.voices) {
        for (uint8_t& b : o.own) b = (uint8_t)rng();
        oscDrawBuiltRamps(o.ramps, rng);
        auto* tail = reinterpret_cast<uint32_t*>(&o.tail);
        for (size_t k = 0; k < sizeof o.tail / 4; ++k) tail[k] = (uint32_t)rng();
    }
    const std::span<const OscEvent> events = oscEvents();
    std::string what;
    for (int k = 0; k < kSteps && what.empty(); ++k, ++s.n.events) {
        drawBefore(s, rng, k == 0);
        what = events[rng() % events.size()](s, rng, (int)(rng() % kOscVoices));
        for (const auto& [kind, counted] : kTallied) s.n.*counted += vc2CountCalls(s.shared.calls, kind);
        for (const Vc2OscVoice& o : s.voices) {
            for (const Vc2PitchRamp& r : o.ramps) s.n.gliding += r.w.on != 0 && r.w.state == 2;
        }
    }
    return what;
}

const char* const kOwnNames[kOscVoices] = { "voice 0's own words", "voice 1's own words" };
const char* const kRampNames[kOscVoices] = { "voice 0's ramps", "voice 1's ramps" };
const char* const kTailNames[kOscVoices] = { "voice 0's tail", "voice 1's tail" };
} // namespace

float oscDrawPosition(std::mt19937& rng) {
    const std::array<float, 4> edges { kTwo31, std::nextafter(kTwo31, 0.0f), 2.0f * kTwo31,
                                       std::nextafter(2.0f * kTwo31, 0.0f) };
    if (rng() % 2 == 0) return edges[rng() % edges.size()];
    return rng() % 2 ? kTwo31 * (0.5f + 1.5f * vc2Unit(rng)) : oscDrawFloat(rng, OscRange::noNaN);
}

void oscWriteVoice(OscSides& s, int v) {
    const Vc2OscVoice& o = s.voices[(size_t)v];
    s.fw.cpu.write(oscVoiceAt(v) + Vc2OscVoice::kOwn, o.own.data(), o.own.size());
    s.fw.cpu.write(oscVoiceAt(v) + kRamps, o.ramps.data(), sizeof o.ramps);
    s.fw.cpu.write(oscVoiceAt(v) + kTail, &o.tail, sizeof o.tail);
}

float oscDrawFloat(std::mt19937& rng, OscRange range) {
    constexpr std::array<float, 8> kEdges { 0.0f, -0.0f, 1.0f, -1.0f, 3e38f, -3e38f, INFINITY, -INFINITY };
    constexpr std::array<float, 4> kScales { 1.0f, 100.0f, 10000.0f, 0.001f };
    // moderate: 0, −0, ±1 and to ±1000, so that an output times its depth reaches exp2 below 2·10^6, where its tables'
    // reads stay in rom.4's memory (the pitch's still pass its ±6000)
    constexpr std::array<float, 4> kModerateScales { 1.0f, 100.0f, 1000.0f, 0.001f };
    constexpr size_t kModerateEdges = 4;
    const bool moderate = range == OscRange::moderate;
    if (rng() % 4 == 0) return kEdges[rng() % (moderate ? kModerateEdges : kEdges.size())];
    if (range == OscRange::any && rng() % 32 == 0) return NAN;
    return (2.0f * vc2Unit(rng) - 1.0f) * (moderate ? kModerateScales : kScales)[rng() % kScales.size()];
}

void oscBlockFloats(std::span<uint32_t> words, std::mt19937& rng) {
    for (uint32_t at : kModerateWords) {
        words[(at - Vc2OscVoice::kOwn) / 4] = std::bit_cast<uint32_t>(oscDrawFloat(rng, OscRange::moderate));
    }
    for (uint32_t at : kFloatWords) words[(at - Vc2OscVoice::kOwn) / 4] = std::bit_cast<uint32_t>(oscDrawFloat(rng));
}

std::string oscDifference(OscSides& s) {
    if (s.romShared.faulted) return "rom.4's exp2 faults for the port";
    for (int v = 0; v < kOscVoices; ++v) {
        const Vc2OscVoice& o = s.voices[(size_t)v];
        const uint32_t at = oscVoiceAt(v);
        const std::string what = vc2RegionsDifference(
            s.fw, { { at + Vc2OscVoice::kOwn, o.own.data(), o.own.size(), kOwnNames[v] },
                    { at + kRamps, o.ramps.data(), sizeof o.ramps, kRampNames[v] },
                    { at + kTail, &o.tail, sizeof o.tail, kTailNames[v] } });
        if (!what.empty()) return what;
    }
    return oscCallsDifference(s.rom, s.shared.calls);
}

static bool loadOscAndRamp(const char* path) { return vc2LoadOsc(path) && vc2LoadRamp(path); }

int main(int argc, char** argv) {
    Vc2Fw fw;
    const auto run = vc2Start(argc, argv, fw, "xt_vc2osc", loadOscAndRamp);
    if (!run) return 1;
    std::array<Vc2OscVoice, kOscVoices> voices {};
    OscShared shared;
    std::vector<OscPortParts> parts;
    for (int v = 0; v < kOscVoices; ++v) parts.emplace_back(v, shared, voices[(size_t)v]);
    std::vector<OscCall> rom;
    const RomShared romShared(fw);
    OscSides s { fw, voices, parts, shared, rom, romShared };
    Vc2Tally tally { "XT_VC2OSC_FIRST", "XT_VC2OSC_STOP" };
    oscRampPart(fw, romShared, *run, tally);
    oscInstallRom(fw, shared.draws, rom);
    vc2RunCases(*run, tally, "", s.n.events, [&](std::mt19937& rng, bool) { return oscVoiceCase(s, rng); });
    const OscCounts& n = s.n;
    std::printf("osc: %d cases, %ld events, %ld note-ons (%ld with a sample, %ld without), %ld updates, %ld renders, "
                "%ld settings, %ld takes, %ld getters\n", run->cases, n.events, n.noteOns, n.sampled, n.plain,
                n.updates, n.renders, n.settings, n.takes, n.getters);
    std::printf("calls: %ld seeks, %ld kinds' routines; %ld ramps gliding after an event\n", n.seeks, n.kindCalls,
                n.gliding);
    std::printf("xt_vc2osc: %ld different\n", tally.different);
    return 0;
}
#endif
