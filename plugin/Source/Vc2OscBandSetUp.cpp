// The band players' set-ups on the OSC voice (v333): BACKING's 0x893cd1d0 and ENSEMBLE's 0x893cc8f0, which VariPhrase's
// start runs on the object for a wave of type 3 or 5 (Vc2OscVari.cpp), and the tail they share, 0x893cd370 (BACKING's
// its own copy of it, the same code).  They read the wave through the object's head (+0x44): its +0 the events' table,
// +4 band 0's samples and 4k on band k's, +0x2c the cached loop events.  rom.2's note-ons 0x89510980 and 0x89510130
// take the same steps otherwise (XtMbPlayer::noteOn).  Every address is rom.4's, every offset the object's but the
// head's.  HARDWARE_FIRMWARE.md, "The VC-2's band players' set-ups and seek".  Audio thread.
#include "Vc2OscPlayers.h"
#include "Vc2OscTables.h"
#include "Vc2OscWalk.h"

namespace {
// [FW] the head's words: band 0's samples (band k's 4k on); v334: the events' table and the cached loop events
// vc2WalkSetUp's (Vc2OscWalkStart.cpp), with the events' layout (kVc2BandLayout)
constexpr uint32_t kSamplesFrom = 0x4;
// [FW] the object's: byte 0 (0x40 stereo), byte 3 (the walk's bits: 0x40 kept, ENSEMBLE's 0x80 band 0's records
// alone), the end, the position ×16, the walk, the gain's rate
constexpr uint32_t kFlags = 0x0, kWalkBits = 0x3, kEnded = 0x4, kPos = 0x38, kWalk = 0x7c, kGainRate = 0x88;
constexpr uint8_t kStereo = 0x40, kKeeps = 0x40, kBandZeroAlone = 0x80;
// [FW] the bands from +0xec, 0x88 apart; a band's words: its data, the position's shift, the walk list's stride, the
// fraction's and the step's shifts (half words), the read's start offset, the mask, the gain, the smoother's two
constexpr uint32_t kBandZero = 0xec, kBandStride = 0x88;
constexpr uint32_t kData = 0x0, kShift = 0x4, kListStride = 0x8, kFractionShift = 0xc, kStepShift = 0xe;
constexpr uint32_t kStartOffset = 0x30, kMask = 0x3c, kGain = 0x40, kSmoothL = 0x44, kSmoothR = 0x48;
// [FW] BACKING's bands 1…5's data two frames before their samples: a mono frame's bytes, a stereo frame's
constexpr uint32_t kTwoMono = 4, kTwoStereo = 8;
// [FW] ENSEMBLE's bands 1…5: the frame, each channel's 12 values (a half word, then its step to the next frame's) from
// +0x10 and +0x4c and its 6 phases (half words) from +0x40 and +0x7c; the frame the position's shifted by 6 past the
// band's shift
constexpr uint32_t kFrameAt = 0xc, kValues = 12, kPhases = 6, kPastShift = 6;
constexpr std::array<uint32_t, 2> kValuesAt { 0x10, 0x4c }, kPhasesAt { 0x40, 0x7c };
// [FW] the tree's state: 25 pairs of floats from +0x51c, the right's from +0x6e4
constexpr std::array<uint32_t, 2> kTreeAt { 0x51c, 0x6e4 };
constexpr uint32_t kTreeFloats = 50;
constexpr uint32_t kBands = Vc2OscBandTables::kBands;

uint32_t bandWordAt(uint32_t k, uint32_t off) { return kBandZero + kBandStride * k + off; }

// [FW] a band's words from the tables, band k's of each: the fraction's shift, the mask, the step's shift, the start
// offset; and the gain
void tabledWords(Vc2WalkWords& w, uint32_t k, float gain) {
    const Vc2OscBandTables& t = vc2OscTables().bands;
    w.setHalf(bandWordAt(k, kFractionShift), t.fractionShift[k]);
    w.setWord(bandWordAt(k, kMask), (int32_t)t.mask[k]);
    w.setHalf(bandWordAt(k, kStepShift), t.stepShift[k]);
    w.setWord(bandWordAt(k, kStartOffset), (int32_t)t.startOffset[k]);
    w.setReal(bandWordAt(k, kGain), gain);
}

// [FW] both heads: byte 3 but its 0x40 cleared, with ENSEMBLE's 0x80 set; the end 0; band 0's samples and words, its
// gain −1 when the walk is below 0, else 1; the gain is the answer
float startHead(Vc2WalkWords& w, Vc2OscVoice& v, Vc2OscParts& p, bool ensemble) {
    const uint8_t bits = w.byte(kWalkBits) & kKeeps;
    w.setByte(kWalkBits, ensemble ? (uint8_t)(bits | kBandZeroAlone) : bits);
    w.setByte(kEnded, 0);
    const float gain = 0.0f > w.real(kWalk) ? -1.0f : 1.0f;
    w.setWord(bandWordAt(0, kData), (int32_t)vc2OscHeadWord(v, p, kSamplesFrom));
    tabledWords(w, 0, gain);
    return gain;
}

// [FW] 0x893cd1d0's bands 1…5: each one's data two frames before its samples, its walk list's stride, its shift k − 1,
// its tabled words and gain, its smoother's two 0
void backingBands(Vc2WalkWords& w, Vc2OscVoice& v, Vc2OscParts& p, float gain) {
    const uint32_t two = (w.byte(kFlags) & kStereo) != 0 ? kTwoStereo : kTwoMono;
    for (uint32_t k = 1; k < kBands; ++k) {
        w.setWord(bandWordAt(k, kData), (int32_t)(vc2OscHeadWord(v, p, kSamplesFrom + 4 * k) - two));
        w.setWord(bandWordAt(k, kListStride), (int32_t)vc2OscTables().bands.stride[k]);
        w.setWord(bandWordAt(k, kShift), (int32_t)k - 1);
        tabledWords(w, k, gain);
        w.setReal(bandWordAt(k, kSmoothL), 0.0f);
        w.setReal(bandWordAt(k, kSmoothR), 0.0f);
    }
}

// [FW] 0x893cc8f0's bands 1…5: each one's data its samples, its frames' shift, its walk list's stride
void ensembleBands(Vc2WalkWords& w, Vc2OscVoice& v, Vc2OscParts& p) {
    for (uint32_t k = 1; k < kBands; ++k) {
        w.setWord(bandWordAt(k, kData), (int32_t)vc2OscHeadWord(v, p, kSamplesFrom + 4 * k));
        w.setWord(bandWordAt(k, kShift), vc2OscTables().bands.frameShift[k]);
        w.setWord(bandWordAt(k, kListStride), (int32_t)vc2OscTables().bands.stride[k]);
    }
}

// [FW] 0x893cd370: the events' table and the cached loop events from the head's words, the gain's rate 1/64, the
// events' layout and a frame's bytes by byte 0's stereo, the walk's start with the band players' row (index 16), the
// tree's state 0, the right's too in stereo
void tail(Vc2WalkWords& w, Vc2OscVoice& v, Vc2OscParts& p) {
    w.setReal(kGainRate, vc2OscTables().bands.gainRate);
    vc2WalkSetUp(v, p, kVc2BandLayout, kVc2BandWalk);   // v334: the table, cached events, layout, walk's start
    const bool stereo = (w.byte(kFlags) & kStereo) != 0;
    for (uint32_t side = 0; side < (stereo ? 2u : 1u); ++side) {
        for (uint32_t i = 0; i < kTreeFloats; ++i) w.setReal(kTreeAt[side] + 4 * i, 0.0f);
    }
}

// [FW] 0x893cc8f0's last: each of bands 1…5 its frame the position's (+0x38) shifted down by its shift + 6, and from
// that frame's 12 values a channel (a channel's values interleaved with the other's in stereo) each value and its step
// to the next frame's, as half words; its phases 0
void ensembleFrames(Vc2WalkWords& w) {
    const uint32_t channels = (w.byte(kFlags) & kStereo) != 0 ? 2 : 1;
    const uint32_t frameBytes = kValues * channels * (uint32_t)sizeof(int16_t);
    for (uint32_t k = 1; k < kBands; ++k) {
        const int32_t frame = w.word(kPos) >> (w.word(bandWordAt(k, kShift)) + (int32_t)kPastShift);
        w.setWord(bandWordAt(k, kFrameAt), frame);
        const uint32_t at = (uint32_t)w.word(bandWordAt(k, kData)) + (uint32_t)frame * frameBytes;
        for (uint32_t c = 0; c < channels; ++c) {
            for (uint32_t j = 0; j < kValues; ++j) {
                const uint32_t from = at + (uint32_t)sizeof(int16_t) * (j * channels + c);
                const int16_t now = w.memoryHalf(from), next = w.memoryHalf(from + frameBytes);
                w.setHalf(bandWordAt(k, kValuesAt[c] + 4 * j), now);
                w.setHalf(bandWordAt(k, kValuesAt[c] + 4 * j + 2), next - now);
            }
            for (uint32_t i = 0; i < kPhases; ++i) w.setHalf(bandWordAt(k, kPhasesAt[c] + 2 * i), 0);
        }
    }
}
} // namespace

// [FW] 0x893cd1d0 and 0x893cc8f0: the head and band 0, the kind's bands 1…5, the tail, the loops' rates (0x893ccf50)
// and the beat's start (0x893cbf40); then ENSEMBLE's first frames
void vc2OscBandSetUp(Vc2OscVoice& v, Vc2OscParts& p, const Vc2ToneShared& shared, bool ensemble) {
    Vc2WalkWords w { v, p };
    const float gain = startHead(w, v, p, ensemble);
    if (ensemble) ensembleBands(w, v, p);
    else backingBands(w, v, p, gain);
    tail(w, v, p);
    vc2OscLoopRates(v, p, shared);
    v.beatStart();
    if (ensemble) ensembleFrames(w);
}
