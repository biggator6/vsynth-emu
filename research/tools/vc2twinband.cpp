// vc2run band <rom.2>: BACKING's and ENSEMBLE's players (rom.2's 0x8950e6e0 and 0x8950db10) on rom.2's and rom.4's
// routines, without the port (xt_fw bandcmp holds XtMbPlayer to rom.2, so rom.2 equal to rom.4 is the port equal to
// rom.4).  Each test wave (vc2twinplay.cpp) is encoded by rom.2's multi-band encoder (xtFwEncodeMulti: one segment, or
// the attack detection at an Encode Depth) and its bands rebuilt by rom.2's load (xtFwLoadBands); a note's player is set
// up as xtFwBandPlay sets it up and copied into rom.4's layout; then each image's note-on and 120 blocks, compared after
// each call, with the walk and gain moved as the note plays and one note in three looped.  Fails on any difference.
#include "XtFirmware.h"
#include "XtMultiBandPlayer.h"
#include "vc2runs.h"
#include "vc2twinplay.h"
#include <array>
#include <cstdio>
#include <cstring>

namespace vc2 {

namespace {

struct BandType {
    int type;
    Twin::Pair noteOn, block;   // [FW] rom.2's and rom.4's: the kind 2 install's literals, codealign.py --fields
    const char* name;
};
constexpr BandType kTypes[] = {
    {3, {0x89510980, 0x893cd1d0}, {0x8950e6e0, 0x893cadc0}, "BACKING"},
    {5, {0x89510130, 0x893cc8f0}, {0x8950db10, 0x893ca1f0}, "ENSEMBLE"},
};
constexpr int kWaves = 10, kNotes = 4, kBlocks = 120;
constexpr int kDepths[] = {-1, 30, 90};   // one segment, or the attack detection at this Encode Depth
constexpr uint32_t kTuning = kWaveStruct + 0x100, kNoBound = 0x7fffffff;
// [FW] a vpdt's events: their count in the header's +8 high half, then each from the 16-bit word kEventsAt, this many
// words apart, its position at +4 (xtFwBandPlay)
constexpr uint32_t kEventsAt = 0xe, kEventWordsMono = 0x2e, kEventWordsStereo = 0x58;

// [FW] the player's fields every note has (xtFwBandPlay): no loop bounds, the wave struct, the rates, the start and end
// events, the gains, the formant 1, energy 0, the loop regions' rate factors 1
constexpr SetField kFixed[] = {
    u16At(0x14, 0),           u16At(0x16, 0xffff),           u32At(0x18, 0),
    u32At(0x28, kNoBound),    u32At(0x2c, kNoBound),         u32At(0x44, kWaveStruct),
    u32At(0x48, 44100),       u32At(0x4c, 44100),            f32At(0x74, 1.0f),
    f32At(0x78, 0.0f),        f32At(0x80, 1.0f / 65536.0f),  f32At(0x84, 1.0f / 65536.0f),
    f32At(0x88, 1.0f / 64.0f), f32At(0x9c, 1.0f),            f32At(0xa0, 1.0f),
    f32At(0xa4, 1.0f),
};

uint32_t vpdtWord(const std::vector<uint8_t>& v, size_t at) {
    uint32_t w = 0;
    if (at + 4 <= v.size()) std::memcpy(&w, v.data() + at, 4);
    return w;
}

// The last event's position in 1/16 sample: the end the player is given
uint32_t lastEventAt(const std::vector<uint8_t>& vpdt, int channels) {
    const uint32_t events = vpdtWord(vpdt, 8) >> 16;
    const uint32_t stride = channels == 2 ? kEventWordsStereo : kEventWordsMono;
    return events ? vpdtWord(vpdt, ((events - 1) * stride + kEventsAt) * 2 + 4) : 0;
}

// The wave in both machines: rom.2's encode of it, its six bands as rom.2's load rebuilds them, the wave struct
bool loadBandWave(Twin& t, int type, const TestWave& w, int depth, std::vector<uint8_t>& vpdt) {
    XtMbBands bands;
    if (!xtFwEncodeMulti(type, w.pcm.data(), w.frames, w.channels, vpdt, depth)) return false;
    if (!xtFwLoadBands(type, w.pcm.data(), w.frames, w.channels, bands)) return false;
    const std::vector<uint8_t> zero(0x200, 0);
    t.write(kWaveStruct, zero.data(), zero.size());
    std::array<uint32_t, 12> head{kVpdt};
    uint32_t at = kSamples;
    for (size_t k = 0; k < std::size(bands.buf); ++k) {
        const size_t bytes = bands.buf[k].size() * 2;
        t.write(at, bands.buf[k].data(), bytes);
        head[1 + k] = at;
        at = (uint32_t)((at + bytes + 0xff) & ~size_t{0xff});
    }
    head[10] = kTuning;
    head[11] = kCache;
    t.write(kWaveStruct, head.data(), sizeof head);
    t.write(kVpdt, vpdt.data(), vpdt.size());
    return true;
}

// The note's loop events into both machines' caches, as xtFwBandPlay builds them; false when there are none
bool loadBandLoop(Twin& t, const TestWave& w, const std::vector<uint8_t>& vpdt, const TestNote& n) {
    const std::vector<uint8_t> zero(kCacheBytes, 0);
    t.write(kCache, zero.data(), zero.size());
    const XtMbWave lw{vpdt.data(), vpdt.size(), w.channels, {}};   // the loop events read the vpdt alone
    uint8_t ev[4][kXtMbLoopEventBytes];
    if (n.loop.on && xtMbLoopEvents(lw, n.loop, ev)) {
        t.write(kCache, ev, sizeof ev);
        return true;
    }
    t.write(kCache + 0x16c, &kNoBound, 4);
    t.write(kCache + 0xb8, &kNoBound, 4);
    return false;
}

// One note: the player set up in rom.2 and copied, each image's note-on, then its blocks; false on a fault
bool bandNote(Twin& t, Rng& r, const BandType& b, const TestWave& w, const std::vector<uint8_t>& vpdt,
              Twin::Returns& n) {
    const TestNote tn = makeTestNote(r, w);
    const bool looped = loadBandLoop(t, w, vpdt, tn);
    const std::vector<uint8_t> zero(kPlayerBytes, 0);
    t.vsynth.write(kPlayer, zero.data(), zero.size());
    setFields(t, kFixed);
    const uint32_t end = lastEventAt(vpdt, w.channels);
    const SetField own[] = {
        u8At(0, 0x80u | (w.channels == 2 ? 0x40u : 0u) | (looped ? 1u : 0u)),
        u32At(0x1c, end), u32At(0x20, end / 16 * 16),
        f32At(0x70, tn.pitch), f32At(0x98, tn.pitch), f32At(kWalk, tn.walk),
    };
    setFields(t, own);
    const PlayerRun run{b.block, kOscMap, b.name};
    t.copyMapped(kPlayer, kPlayerBytes + kOscInsert.bytes, kOscMap);
    if (!t.call(b.noteOn, {kPlayer, 0})) return false;
    tallyCall(t, run, false, b.name, n);
    return playBlocks(t, run, kBlocks, makeMoves(r, kBlocks), n);
}

int bandCases(Twin& t) {
    Rng r;
    Twin::Returns all;
    for (const BandType& b : kTypes) {
        Twin::Returns n;
        for (int k = 0; k < kWaves; ++k) {
            const TestWave w = makeTestWave(r);
            std::vector<uint8_t> vpdt;
            if (!loadBandWave(t, b.type, w, kDepths[k % std::size(kDepths)], vpdt)) {
                std::printf("%s: rom.2's encode or load of wave %d failed\n", b.name, k);
                return -1;
            }
            for (int j = 0; j < kNotes; ++j)
                if (!bandNote(t, r, b, w, vpdt, n)) return -1;
        }
        addCount(b.name, "calls", n, all);
    }
    return sayCount("band", "calls", all);
}

} // namespace

int band(Machine& m, int argc, char** argv) {
    return twinRun(m, argc, argv, xtFwOpen, bandCases);
}

} // namespace vc2
