// Kind 1's mode 1's render, rom.4's 0x893c9f60 (v327): rom.2's 0x8950d880 on rom.4's object (its words four bytes up
// from +0x94: the rate factor +0xa0, the samples' address +0xec), mono and stereo by the object's byte 0.  The beat's
// block gives the step's word; the walk (Vc2OscWalk.cpp) moves it through the events and loops.  HARDWARE_FIRMWARE.md,
// "The VC-2's mode 1".  Every address is rom.4's, every offset the object's unless named the voice's.
#include "Vc2OscPlayers.h"
#include "Vc2OscTables.h"
#include "Vc2OscWalk.h"
#include "XtFloat.h"

namespace {
using xtf::add;
using xtf::flt;
using xtf::mul;
using xtf::sub;

// [FW] the object's: its flags (0x40 stereo), the end (1 past the range, −1 before it), the range's start and end,
// the next crossing, the step, the position (×16) and its fraction (20 bits), the pitch, the rate factor, the samples'
// address
constexpr uint32_t kFlags = 0x0, kEnded = 0x4, kStart = 0x18, kEnd = 0x1c, kCross = 0x2c, kStep = 0x34;
constexpr uint32_t kPos = 0x38, kFrac = 0x3c, kPitch = 0x70, kRate = 0xa0, kSamples = 0xec;
// [FW] the voice's: the gain's target, the gain and its rate (the object's −0x80, −0x7c, −0x78)
constexpr uint32_t kTarget = 0xdc, kGain = 0xe0, kGainRate = 0xe4;
constexpr uint8_t kStereo = 0x40, kBefore = 0xff;
constexpr int kFrames = 32, kFracBits = 20, kSixteenths = 4;   // [FW] a block; the fraction's bits; ×16
constexpr uint32_t kFracMask = 0xfffff, kFrac24 = 0xffffff;
constexpr uint32_t kMonoShift = 1, kStereoShift = 2, kMonoNext = 2, kStereoNext = 4, kRight = 2;   // [FW] half words

// [FW] crossing the next crossing: the walk's crossing that way (index 0's row) and the next crossing found; what the
// frame then compares, forward the position, back the crossing (rom.4's r1 after 0x893c9380, not the position)
int32_t walked(Vc2WalkWords& w, bool forward) {
    (forward ? vc2WalkForward : vc2WalkBackward)(w.v, w.p, kVc2ModeOneWalk);
    vc2WalkCrossing(w.v, w.p);
    return w.word(forward ? kPos : kCross);
}

// [FW] 0x893c9fa4…: a frame's move by half the step, forward or back, inside the range; crossing the next crossing,
// walked; at or past the range's end (forward +0x1c, back +0x18) the position held there, its fraction 0, ended
void move(Vc2WalkWords& w) {
    const int32_t step = w.word(kStep);
    const bool forward = step >= 0;
    int32_t at = w.word(kPos);
    if (forward ? w.word(kEnd) > at : at > w.word(kStart)) {
        const int32_t frac = vc2WalkPlus(w.word(kFrac), step >> 1);
        at = vc2WalkPlus(at, frac >> kFracBits);
        w.setWord(kPos, at);
        w.setWord(kFrac, (int32_t)((uint32_t)frac & kFracMask));
        if (forward ? at >= w.word(kCross) : w.word(kCross) >= at) at = walked(w, forward);
    }
    if (!(forward ? at >= w.word(kEnd) : w.word(kStart) >= at)) return;
    w.setWord(kPos, w.word(forward ? kEnd : kStart));
    w.setWord(kFrac, 0);
    w.setByte(kEnded, forward ? 1 : kBefore);
}

// [FW] 0x893ca060…: between two samples by the position's sixteenths and fraction, over 2^24
float fraction(const Vc2WalkWords& w) {
    const uint32_t at = (uint32_t)w.word(kPos);
    const uint32_t f = ((at << kFracBits) + (uint32_t)w.word(kFrac)) & kFrac24;
    return mul(flt((int)f), vc2OscTables().modeOneFraction);
}
float between(int a, int b, float f) {
    const float s = flt(a);
    return add(mul(sub(flt(b), s), f), s);
}
// [FW] the gain gliding to its target by its rate (FMAC), × 65536
float gain(Vc2OscVoice& v) {
    const float g = add(mul(v.f32(kGainRate), sub(v.f32(kTarget), v.f32(kGain))), v.f32(kGain));
    v.setF32(kGain, g);
    return mul(g, vc2OscTables().modeOneScale);
}

// [FW] 0x893ca080…: a frame, mono (both sides the one sample) or stereo (half words left, right), at the gain
void frame(Vc2WalkWords& w, const Vc2OscSamples& wave, float* lr) {
    const float f = fraction(w);
    const bool stereo = (w.byte(kFlags) & kStereo) != 0;
    const uint32_t index = (uint32_t)w.word(kPos) >> kSixteenths;
    const uint32_t at = (uint32_t)w.word(kSamples) + (index << (stereo ? kStereoShift : kMonoShift));
    const uint32_t next = stereo ? kStereoNext : kMonoNext;
    const float l = between(wave.at(at), wave.at(at + next), f);
    const float r = stereo ? between(wave.at(at + kRight), wave.at(at + kRight + next), f) : l;
    const float g = gain(w.v);
    lr[0] = mul(l, g);
    lr[1] = mul(r, g);
}
} // namespace

// [FW] 0x893c9f60: the beat's block (0x893c89a0, Vc2OscVoice::beatBlock) gives the step's word, scaled by the pitch ×
// the rate factor; the next crossing; then 32 frames, each moved, then read
void vc2OscModeOne(Vc2OscVoice& v, float* out, Vc2OscParts& p) {
    Vc2WalkWords w { v, p };
    v.beatBlock(p);
    const float by = mul(w.real(kPitch), w.real(kRate));
    w.setWord(kStep, xtf::ftrc(mul(flt(w.word(kStep)), by)));
    vc2WalkCrossing(v, p);
    const Vc2OscSamples wave = p.waveSamples();
    for (int k = 0; k < kFrames; ++k) {
        move(w);
        frame(w, wave, out + 2 * k);
    }
}
