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
constexpr uint32_t kRamps = 0x90c, kTail = 0xa38;   // [FW] where the ramps (v236) and the tail are in a voice
constexpr size_t kEnvelopeBinds = 4;
constexpr std::array<uint32_t, 4> kRenders { 0x893c62e0, 0x893c61e0, 0, 0 };   // [FW] the install's two, and two hooked
// [FW] the words each routine decides by: the kind, the wave's type, the block's byte +0x138, the object's flags, the
// source's byte +4, its seek word, level, +0x168, positions, the legato word, the take's gate; and the block's floats
// the pitch, the ramp's start and the targets read
constexpr uint32_t kKind = 0x54, kWave = 0x104, kFixed = 0x138, kFlags = 0x15c, kSourceByte = 0x160, kSeekAt = 0x1c0;
constexpr uint32_t kLevel = 0x1e0, kHalf = 0x168, kPosition = 0x194, kPositionLite = 0x24c, kPositionFloat = 0x270;
constexpr uint32_t kLegato = 0x908, kGate = 0xb8;
constexpr uint32_t kHeaderAt = 0x128, kLoopAt = 0x130, kPairAt = 0x134;   // [FW] the block's +0xd4, +0xdc, +0xe0
// The calls counted over the run, by kind
constexpr std::array<std::pair<OscKind, long OscCounts::*>, 3> kTallied { {
    { kSeek, &OscCounts::seeks }, { kEventPosition, &OscCounts::lookups }, { kObjectCall, &OscCounts::objectCalls } } };

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

} // namespace

// v238: the block's record pointers VariPhrase's note-on reads through (the header's, the loop record's, the +0xe0
// record's), drawn before every event: kind 5's settings copy 88 bytes over them (its P), and a kind then steered to 2
// would take its note-on through them, which the tone never does without a block of its own.  From v321 drawn again
// when the kinds' note-on makes a voice a player: kinds 5's and 6's words lie over them (Vc2OscDraws.cpp)
void oscDrawRecordWords(Vc2OscVoice& o, std::mt19937& rng) {
    const std::array<std::pair<uint32_t, uint32_t (*)(int)>, 3> records { { { kHeaderAt, oscHeaderAt },
                                                                           { kLoopAt, oscLoopAt },
                                                                           { kPairAt, oscPairAt } } };
    for (const auto& [at, where] : records) o.setU32(at, where((int)(rng() % kOscRecords)));
}

namespace {
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
    // v242: the word the analog's LA phases hold their terms against (0x893c4a20), which nothing writes: no NaN
    constexpr uint32_t kAnalogUnwritten = 0x298;
    if (now()) o.setF32(kAnalogUnwritten, oscDrawFloat(rng, OscRange::moderate));
    for (const auto& [at, range] : oscBlockFloatWords()) {
        if (now()) o.setF32(at, oscDrawFloat(rng, range));
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
    oscDrawRecordWords(o, rng);
}

// The vibrato's outputs as the targets find them, wider than its own update leaves them: the pitch's moderate, as it
// reaches exp2; the level's any, but within ±1 on kinds 5 and 6, whose gains it reaches.  Its block's words as the
// converter could write them, which a kind's note-on reads
void steerVibrato(Vc2OscVoice& o, std::mt19937& rng) {
    o.vibrato.w.pitch = oscDrawFloat(rng, OscRange::moderate);
    o.vibrato.w.level = oscIsVoiced(o.kind()) ? 2.0f * vc2Unit(rng) - 1.0f : oscDrawFloat(rng);
    oscDrawVibratoVoice(o, rng);
}

// The draws before an event, written on both sides: the answers, the records, the steered words, each voice's own
// words, vibrato and tail into rom.4's memory
void drawBefore(OscSides& s, std::mt19937& rng, bool all) {
    oscDrawAnswers(s.fw, s.shared, rng);
    oscDrawRecords(s.shared.records, rng);
    oscDrawLoopEvents(s.shared.records, s.shared.renders);   // v326
    oscWriteRecords(s.fw, s.shared.records);
    for (Vc2OscVoice& o : s.voices) {
        steer(o, rng, all);
        oscDrawVoicedVoice(o, rng);   // v266: kinds 5 and 6 on what a note gives them
        steerVibrato(o, rng);         // v267
    }
    for (int v = 0; v < kOscVoices; ++v) oscWriteVoice(s, v);
    s.rom.clear();
}

// A case: both voices the same noise, their ramps as glides leave them (each of its built type: the voice restarts the
// pitch's from its own words, and a ratio's ramp from those would take logf to a NaN), then its events
std::string oscVoiceCase(OscSides& s, std::mt19937& rng) {
    for (Vc2OscVoice& o : s.voices) {
        for (uint8_t& b : o.own) b = (uint8_t)rng();
        oscDrawBuiltRamps(o.ramps, rng);
        oscDrawVibratoWords(o.vibrato.w, rng);
        auto* tail = reinterpret_cast<uint32_t*>(&o.tail);
        for (size_t k = 0; k < sizeof o.tail / 4; ++k) tail[k] = (uint32_t)rng();
    }
    const std::span<const OscEvent> events = oscEvents();
    oscDrawWave(s.fw, s.shared);               // v323: the wave Vari off's renders play
    std::string what = oscKindsCase(s, rng);   // v266: the mic ring, and each voice's carrier and kind 6 started
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

const char* const kVibratoNames[kOscVoices] = { "voice 0's vibrato", "voice 1's vibrato" };
const char* const kOwnNames[kOscVoices] = { "voice 0's own words", "voice 1's own words" };
const char* const kRampNames[kOscVoices] = { "voice 0's ramps", "voice 1's ramps" };
const char* const kTailNames[kOscVoices] = { "voice 0's tail", "voice 1's tail" };
} // namespace

void oscWriteVoice(OscSides& s, int v) {
    const Vc2OscVoice& o = s.voices[(size_t)v];
    s.fw.cpu.write(oscVoiceAt(v), &o.vibrato.w, sizeof o.vibrato.w);
    s.fw.cpu.write(oscVoiceAt(v) + Vc2OscVoice::kOwn, o.own.data(), o.own.size());
    s.fw.cpu.write(oscVoiceAt(v) + kRamps, o.ramps.data(), sizeof o.ramps);
    s.fw.cpu.write(oscVoiceAt(v) + kTail, &o.tail, sizeof o.tail);
}

std::string oscDifference(OscSides& s) {
    if (s.romShared.faulted) return "rom.4's exp2 faults for the port";
    for (int v = 0; v < kOscVoices; ++v) {
        const Vc2OscVoice& o = s.voices[(size_t)v];
        const uint32_t at = oscVoiceAt(v);
        const std::string what = vc2RegionsDifference(
            s.fw, { { at, &o.vibrato.w, sizeof o.vibrato.w, kVibratoNames[v] },
                    { at + Vc2OscVoice::kOwn, o.own.data(), o.own.size(), kOwnNames[v] },
                    { at + kRamps, o.ramps.data(), sizeof o.ramps, kRampNames[v] },
                    { at + kTail, &o.tail, sizeof o.tail, kTailNames[v] } });
        if (!what.empty()) return what;
    }
    return oscCallsDifference(s.rom, s.shared.calls);
}

int main(int argc, char** argv) {
    Vc2Fw fw;
    const auto run = vc2Start(argc, argv, fw, "xt_vc2osc",
                              vc2LoadAll<vc2LoadOsc, vc2LoadRamp, vc2LoadCarrier, vc2LoadKind6>);
    if (!run) return 1;
    std::array<Vc2OscVoice, kOscVoices> voices {};
    OscShared shared;
    shared.renders.seed(~(uint32_t)run->seed);   // v322: the renders' generator of its own, from the run's seed
    oscFindZeroTunes(fw, shared.records);
    std::vector<OscPortParts> parts;
    for (int v = 0; v < kOscVoices; ++v) parts.emplace_back(v, shared, voices[(size_t)v]);
    std::vector<OscCall> rom;
    const RomShared romShared(fw);
    OscSides s { fw, voices, parts, shared, rom, romShared };
    Vc2Tally tally { "XT_VC2OSC_FIRST", "XT_VC2OSC_STOP" };
    oscRampPart(fw, romShared, *run, tally);
    oscVariOffPart(fw, *run, tally);
    oscVibratoPart(fw, *run, tally);
    oscInstallRom(fw, shared.draws, rom, oscVoiceNumber);
    oscModeOnePart(s, *run, tally);   // v327
    const std::string setUp = oscKindsSetUp(s);
    vc2RunCases(*run, tally, setUp, s.n.events, [&](std::mt19937& rng, bool) { return oscVoiceCase(s, rng); });
    const OscCounts& n = s.n;
    std::printf("osc: %d cases, %ld events, %ld note-ons (%ld with a sample, %ld without), %ld updates, %ld renders, "
                "%ld settings, %ld takes, %ld getters\n", run->cases, n.events, n.noteOns, n.sampled, n.plain,
                n.updates, n.renders, n.settings, n.takes, n.getters);
    std::printf("kinds 5 and 6: %ld carriers started, %ld carrier blocks, %ld carrier settings, %ld kind 6 starts, %ld "
                "kind 6 blocks; %ld of the blocks sounding, %ld not finite\n", n.carrierStarts, n.carrierBlocks,
                n.carrierSettings, n.kind6Starts, n.kind6Blocks, n.voicedSounding, n.voicedNotFinite);
    std::printf("calls: %ld seeks, %ld event lookups, %ld players' set-ups; %ld ramps gliding after an event; %ld "
                "VariPhrase words drawn finite\n", n.seeks, n.lookups, n.objectCalls, n.gliding, n.variDrawn);   // v330
    std::printf("own renders: %ld blocks through the EXT IN player's, %ld through Vari off's, %ld through LITE's "
                "heads', %ld through mode 1's\n", n.extInBlocks, n.variOffBlocks, n.headsBlocks,
                n.modeOneBlocks);   // v322, v323, v326, v327
    std::printf("mode 1's set-ups: %ld (%ld with the range in samples); %ld events steered for its seek\n",
                n.modeOneSetUps, n.setUpsInSamples, n.modeOneSeeks);   // v328, v329
    std::printf("lite: %ld updates and %ld settings on a type with a way\n", n.liteUpdates, n.liteSettings);   // v324
    std::printf("xt_vc2osc: %ld different\n", tally.different);
    return 0;
}
#endif
