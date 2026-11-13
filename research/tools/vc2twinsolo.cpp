// vc2run solo <rom.2>: SOLO's player (rom.2's 0x8950cd90) on rom.2's and rom.4's routines, without the port (xt_fw
// solocmp holds XtSoloVoice to rom.2, so rom.2 equal to rom.4 is the port equal to rom.4).  Each test wave
// (vc2twinplay.cpp) is encoded by rom.2's SOLO encoder (xtFwSoloEncode) and laid out as a one-segment vpdt with its loop
// events (xtFwSoloImage); a note's player is set up as xtFwSoloPlay sets it up, copied into rom.4's layout, and started
// by each image's start (0x8950fa60) and event jump (0x8950d580); the rest of the note-on as xtFwSoloPlay writes it; then
// 120 blocks, compared after each call, with the walk and gain moved, one note in three looped and one in four Robot
// Voice.  Fails on any difference.
#include "XtFirmware.h"
#include "vc2runs.h"
#include "vc2twinplay.h"
#include <cstdio>
#include <utility>

namespace vc2 {

namespace {

// [FW] rom.2's and rom.4's: the start, the event jump and the block (the kind 2 install's literals, codealign.py --fields)
constexpr Twin::Pair kStart{0x8950fa60, 0x893cc1a0}, kJump{0x8950d580, 0x893c9c40}, kBlock{0x8950cd90, 0x893c9450};
constexpr int kWaves = 12, kNotes = 4, kBlocks = 120;
constexpr uint32_t kNoBound = 0x7fffffff;
// [FW] the player's two state blocks (xtFwSoloPlay's R9 and R10), by rom.2's offsets; rom.4's R9 is past the insert
constexpr uint32_t kR9 = 0xe8, kR10 = kR9 + 0x3c;
constexpr uint32_t kPeriodAt44k = 12799997;   // (int)(5.644799e11 / 44100), as xtFwSoloPlay writes +0x50
// [FW] SOLO's object in rom.4's layout (codealign.py --fields on 0x8950cde0, 0x8950d0f0, 0x8950d380 and their pairs, and
// each value after the start and jump found in rom.4's): the OSC object's insert at +0x94, so rom.4's R10 is 4 further;
// R10's first word gone (rom.2's 256, or 0 under Robot Voice, from 0x8950c7e0; rom.4's 0x893c8f00 writes none); R10+0x24
// (the note-on's "no cutoff") moved past +0x48: rom.4 has R10+4…+0x20 4 lower, +0x28…+0x48 8 lower, +0x24 at +0x44, +0x4c
// on 4 lower; and a word of its own at +0x188 (its R10+0x60), written at the start, that rom.2 has not
constexpr uint32_t kSoloOwn = 0x188;
constexpr Twin::Segment kSoloMap[] = {
    {0, 0, kOscInsert.at},
    {kOscInsert.at, kOscInsert.at + kOscInsert.bytes, kR10 - kOscInsert.at},
    {kR10 + 0x04, kR10 + 0x04, 0x20},
    {kR10 + 0x28, kR10 + 0x24, 0x24},
    {kR10 + 0x24, kR10 + 0x48, 4},
    {kR10 + 0x4c, kR10 + 0x4c, kSoloOwn - kR10 - 0x4c},
    {kSoloOwn + 4, kSoloOwn + 4, kPlayerBytes - kSoloOwn - 4},
};
constexpr PlayerRun kSolo{kBlock, kSoloMap, "SOLO"}, kRobot{kBlock, kSoloMap, "SOLO, Robot Voice"};

// [FW] the player's fields every note has (xtFwSoloPlay, as EmuSolo.java leaves 0x8950fba0's): the rates and periods,
// the vpdt, the cache, the gains, the formant's and the loops' factors 1, the samples at R9
constexpr SetField kSoloFixed[] = {
    u8At(3, 0x80),            u8At(7, 8),                    u32At(0x10, kVpdt),
    u32At(0x18, 0),           u32At(0x48, 44100),            u32At(0x4c, 44100),
    u32At(0x50, kPeriodAt44k), u32At(0x54, 2 * kPeriodAt44k), u32At(0x6c, kCache),
    f32At(0x80, 1.0f / 65536.0f), f32At(0x84, 1.0f / 65536.0f), f32At(0x88, 1.0f / 64.0f),
    f32At(0x8c, 1.0f),        f32At(0x90, 1.0f),             f32At(0x9c, 1.0f),
    f32At(0xa0, 1.0f),        f32At(0xa4, 1.0f),             u32At(kR9, kSamples),
};
// [FW] the rest of 0x8950fba0 as xtFwSoloPlay writes it after the event jump: words copied (to, from)
constexpr std::pair<uint32_t, uint32_t> kTailCopies[] = {
    {kR10 + 0x18, kR9 + 0x20}, {kR10 + 0x1c, kR9 + 0x20}, {kR10 + 0x2c, kR10 + 0x28}, {kR10 + 0x30, kR10 + 0x28},
    {kR10 + 0x38, kR10 + 0x34}, {kR10 + 0x3c, kR10 + 0x34}, {kR9 + 0x34, kR9 + 0x2c}, {kR9 + 0x38, kR9 + 0x2c},
};
// and set: B's window at 0.5, q 1.2, no cutoff
constexpr SetField kTailSets[] = {
    f32At(kR10 + 0x44, 0.0f), f32At(kR10 + 0x48, 0.5f), f32At(kR10 + 0x24, 0.0f),
    f32At(kR10 + 0x4c, 0.0f), f32At(kR10 + 0x50, 1.2f),
};

// The wave in both machines: its samples, and the vpdt and loop events xtFwSoloImage lays out from rom.2's records
void loadSoloWave(Twin& t, const TestWave& w, const std::vector<XtRecord>& recs, const XtLoop& loop) {
    std::vector<uint8_t> vpdt;
    uint8_t ev[4][kXtFwSoloEventBytes];
    xtFwSoloImage(recs, w.frames, w.channels, loop, vpdt, ev);
    t.write(kSamples, w.pcm.data(), w.pcm.size() * 2);
    t.write(kVpdt, vpdt.data(), vpdt.size());
    const std::vector<uint8_t> zero(kCacheBytes, 0);
    t.write(kCache, zero.data(), zero.size());
    if (!loop.on) {
        t.write(kCache + 0x16c, &kNoBound, 4);
        t.write(kCache + 0xb8, &kNoBound, 4);
        return;
    }
    for (uint32_t k = 0; k < 4; ++k) t.write(kCache + 0xb4 * k, ev[k], kXtFwSoloEventBytes);
}

// The rest of the note-on in rom.2's player (the timer and ages from the record start, both grains there)
void soloTail(Twin& t, const TestWave& w) {
    Sh4& c = t.vsynth;
    const uint32_t start = c.rd32(kPlayer + kR9 + 0x20);
    const uint32_t age = (c.rd32(kPlayer + kR9 + 0x28) - start) * 256u;
    const SetField ages[] = {u32At(kR10 + 0xc, age), u32At(kR10 + 0x10, age), u32At(kR10 + 0x14, age),
                             u32At(kR10 + 0x20, (uint32_t)w.frames - 1)};
    setFields(t, ages);
    for (const auto& [to, from] : kTailCopies) c.wr32(kPlayer + to, c.rd32(kPlayer + from));
    setFields(t, kTailSets);
}

// The event jump on each image, given its own R9: rom.4's is past the insert
bool jump(Twin& t) {
    return t.callVSynth(kJump.vsynth, {kPlayer, kPlayer + kR9}) &&
           t.vc2.run(kJump.vc2, {kPlayer, kPlayer + kR9 + kOscInsert.bytes});
}

// One note: the player set up in rom.2 and copied, each image's start and jump, the tail, then its blocks; counted in
// `counts` (plain, then Robot Voice)
bool soloNote(Twin& t, Rng& r, const TestWave& w, const std::vector<XtRecord>& recs, Twin::Returns (&counts)[2]) {
    const TestNote tn = makeTestNote(r, w);
    const float formant = r.in(0.5f, 2.0f), energy = r.in(0.0f, 1.0f);
    const bool robot = r.next() % 4 == 0;
    const PlayerRun& run = robot ? kRobot : kSolo;
    Twin::Returns& n = counts[robot ? 1 : 0];
    loadSoloWave(t, w, recs, tn.loop);
    const std::vector<uint8_t> zero(kPlayerBytes, 0);
    t.vsynth.write(kPlayer, zero.data(), zero.size());
    setFields(t, kSoloFixed);
    const uint32_t end = (uint32_t)w.frames * 16, stereo = w.channels == 2 ? 1u : 0u;
    const SetField own[] = {
        u8At(0, 0x80u | stereo * 0x40u | (robot ? 0x10u : 0u) | (tn.loop.on ? 1u : 0u)),
        u8At(8, stereo ? 16 : 10), u8At(9, stereo ? 12 : 6), u32At(0x1c, end), u32At(0x20, end),
        f32At(0x70, tn.pitch), f32At(0x98, tn.pitch), f32At(0x74, formant), f32At(0x78, energy),
        f32At(kWalk, tn.walk),
    };
    setFields(t, own);
    t.copyMapped(kPlayer, kPlayerBytes, kSoloMap);
    if (!t.call(kStart, {kPlayer, 1})) return false;
    tallyCall(t, run, false, "SOLO start", n);
    if (!jump(t)) return false;
    tallyCall(t, run, false, "SOLO event jump", n);
    soloTail(t, w);
    t.copyMapped(kPlayer, kPlayerBytes, kSoloMap);
    return playBlocks(t, run, kBlocks, makeMoves(r, kBlocks), n);
}

int soloCases(Twin& t) {
    Rng r;
    Twin::Returns all, counts[2];
    for (int k = 0; k < kWaves; ++k) {
        const TestWave w = makeTestWave(r);
        std::vector<XtRecord> recs;
        if (!xtFwSoloEncode(w.pcm.data(), (int)w.frames, recs, w.channels) || recs.empty()) {
            std::printf("SOLO: rom.2's encode of wave %d failed\n", k);
            return -1;
        }
        for (int j = 0; j < kNotes; ++j)
            if (!soloNote(t, r, w, recs, counts)) return -1;
    }
    addCount(kSolo.name, "calls", counts[0], all);
    addCount(kRobot.name, "calls", counts[1], all);
    return sayCount("solo", "calls", all);
}

} // namespace

int solo(Machine& m, int argc, char** argv) {
    return twinRun(m, argc, argv, xtFwOpen, soloCases);
}

} // namespace vc2
