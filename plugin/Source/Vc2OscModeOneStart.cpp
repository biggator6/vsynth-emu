// Kind 1's mode 1's set-up, rom.4's 0x893cc760 (v328), which LITE's install runs for a wave of type 1, and the loops'
// rates' 0x893ccf50 it calls.  It reads the wave's records through the object's head (+0x44): the voice's own +0x108
// as its note-on leaves it (Vc2OscPlayers.cpp's vc2OscPlayerHead), or after a legato's take the earlier tone's
// voice's, whose object the take brings (vc2OscHeadWord).  Every address is rom.4's, every offset the object's but
// the head's.
#include "Vc2OscPlayers.h"
#include "Vc2OscTables.h"
#include "Vc2OscWalk.h"

namespace {
// [FW] the head's words (its +0 the block's +0x108): the events' table, the samples, the header, the loop record, the
// +0xe0 record (the loops' cached events)
constexpr uint32_t kTableFrom = 0x0, kSamplesFrom = 0x4, kHeaderFrom = 0x20, kLoopFrom = 0x28, kPairFrom = 0x2c;
// [FW] the object's: its flags (0x40 stereo), the walk's bits (0x70 kept), the end, the events' first, stride and a
// frame's bytes, the table, the loops' record, the samples, the word set 1/64, the rate factor and the loops'
constexpr uint32_t kFlags = 0x0, kWalkBits = 0x3, kEnded = 0x4, kFirst = 0x7, kStride = 0x8, kFrame = 0x9;
constexpr uint32_t kTable = 0x10, kRecord = 0x6c, kSamples = 0xec, kSetWord = 0x88, kRate = 0xa0, kFwdRate = 0xa4;
constexpr uint32_t kBackRate = 0xa8;
constexpr uint8_t kStereo = 0x40, kKeeps = 0x70;

// [FW] the events' layout by the header's format (+1: 3…5 the wider) and the samples' frame: the first half word, the
// stride, a frame's bytes
struct Layout {
    uint8_t first, mono, stereo;
};
constexpr Layout kNarrow { 8, 10, 16 }, kWide { 14, 46, 88 };
constexpr uint8_t kMonoFrame = 6, kStereoFrame = 12;
constexpr int8_t kWideLeast = 3, kWideMost = 5;
} // namespace

// [FW] 0x893ccf50: the rate factor 1, the loops' from the loop record's (+0x14, +0x16) by exp2 over 12000; from v333
// the band players' set-ups' too
void vc2OscLoopRates(Vc2OscVoice& v, Vc2OscParts& p, const Vc2ToneShared& shared) {
    Vc2WalkWords w { v, p };
    const Vc2OscLoopRecord loop = w.p.loopRecord(vc2OscHeadWord(w.v, w.p, kLoopFrom));
    const float tenths = vc2OscTables().modeOneTenths;
    w.setReal(kRate, 1.0f);
    w.setReal(kFwdRate, vc2OscLoopRate(loop.rateFwd, shared, tenths));
    w.setReal(kBackRate, vc2OscLoopRate(loop.rateBwd, shared, tenths));
}

// [FW] 0x893cc760: the table, the loops' record and the samples from the head's words; the walk's bits but 0x70
// cleared, the end 0, +0x88 1/64; the events' layout by the header's format and the frame; then the walk's start with
// mode 1's row, the loops' rates and the beat's start
void vc2OscModeOneSetUp(Vc2OscVoice& v, Vc2OscParts& p, const Vc2ToneShared& shared) {
    Vc2WalkWords w { v, p };
    w.setWord(kTable, (int32_t)vc2OscHeadWord(v, p, kTableFrom));
    w.setWord(kRecord, (int32_t)vc2OscHeadWord(v, p, kPairFrom));
    w.setWord(kSamples, (int32_t)vc2OscHeadWord(v, p, kSamplesFrom));
    w.setByte(kWalkBits, w.byte(kWalkBits) & kKeeps);
    w.setByte(kEnded, 0);
    w.setReal(kSetWord, vc2OscTables().modeOneSetUpWord);
    const int8_t format = (int8_t)p.waveHeader(vc2OscHeadWord(v, p, kHeaderFrom)).format;
    const Layout& l = format >= kWideLeast && format <= kWideMost ? kWide : kNarrow;
    const bool stereo = (w.byte(kFlags) & kStereo) != 0;
    w.setByte(kFirst, l.first);
    w.setByte(kStride, stereo ? l.stereo : l.mono);
    w.setByte(kFrame, stereo ? kStereoFrame : kMonoFrame);
    vc2WalkStart(v, p, kVc2ModeOneWalk);
    vc2OscLoopRates(v, p, shared);
    v.beatStart();
}
