// The period-jump player's render on the OSC voice (v335): rom.4's 0x893ca8b0, rom.2's 0x8950e1d0 on rom.4's object
// (Vc2OscJump.h): a block on the walk, its head the walk's and the read's steps by the record's budget, each frame the
// walk's step (the position, the walk's crossings, the records), the read's (within a period of the walk, jumping a
// period), its two taps a period apart crossfaded, the gain.  HARDWARE_FIRMWARE.md, "The VC-2's period-jump player".
// Every address is rom.4's, every offset the object's unless named the voice's.  Audio thread.
#include "Vc2OscJump.h"
#include "Vc2OscPlayers.h"
#include "Vc2OscTables.h"
#include "XtFloat.h"

namespace vc2jump {
namespace {
using namespace vc2walk;
using xtf::add;
using xtf::div;
using xtf::flt;
using xtf::mul;
using xtf::sub;
constexpr int kSixteenths = 4;   // [FW] ×16
constexpr uint32_t kLowBits = 15;
constexpr int32_t kOneSample = 0x2000000;   // [FW] a step of a sample a frame (2^25), a record's of no budget
constexpr uint8_t kStereo = 0x40;   // [FW] byte 0's
constexpr uint32_t kMonoShift = 1, kStereoShift = 2, kMonoNext = 2, kStereoNext = 4, kRight = 2;   // [FW] half words

// [FW] 0x893ca8d2…: the beat's step taken; a record of no budget walks the step's way and reads a sample a frame; else
// the walk the beat's step, the read the rate factor × the pitch × 2^25
void blockSteps(Vc2WalkWords& w) {
    w.setWord(kBeatStep, w.word(kStep));
    if (w.half(kBudget) == 0) {
        w.setWord(kWalkStep, w.word(kStep) >= 0 ? kOneSample : -kOneSample);
        w.setWord(kReadRate, kOneSample);
        return;
    }
    w.setWord(kWalkStep, w.word(kBeatStep));
    w.setWord(kReadRate, xtf::ftrc(mul(mul(w.real(kRate), w.real(kPitch)), vc2OscTables().jump.rateScale)));
}

// [FW] 0x893ca920…: half the walk's step onto the position; forward (at 0 too), held at the range's end, past the next
// crossing the walk's, past the record's end the next record; or back, held at its start, at or before the next
// crossing the walk's, below the record's start the record before.  After the walk both ways compare the new crossing,
// rom.4's r1 after 0x893c9380 (which stores it at +0x2c), where SOLO's and mode 1's frames take the position forward
void step(Vc2WalkWords& w) {
    const int32_t walk = w.word(kWalkStep);
    const bool forward = walk >> 1 >= 0;
    int32_t frac = w.word(kFrac);
    const int32_t at = vc2WalkHalfStep(w.word(kPos), frac, walk);
    int32_t pos = vc2WalkHeld(w, at, frac, w.word(forward ? kRangeEnd : kRangeStart), forward);
    if (forward ? pos >= w.word(kCross) : w.word(kCross) >= pos) {
        vc2WalkCrossed(w, kVc2JumpWalk, forward);
        pos = w.word(kCross);
    }
    if (forward && pos >= w.word(kRecEnd)) loadNextFloat(w);
    if (!forward && w.word(kRecStart) > pos) loadPrevFloat(w);
}

// [FW] 0x893ca9ae…: the read half its step on; the period by how far into the record the position is, from the
// record's length toward the next's (FMAC); the read kept within a period of where it may be, the position (at the
// least the period, at the most the last event's position less it): past it a period back, a period or more behind it
// a period on (ended, to that place or a period behind it); the crossfade by how far behind the position the read is
// in periods, held to ⅖…⅗, into 0…1
float read(Vc2WalkWords& w) {
    const Vc2OscJumpPool& k = vc2OscTables().jump;
    int32_t frac = w.word(kReadFrac);
    int32_t at = vc2WalkHalfStep(w.word(kRead), frac, w.word(kReadRate));
    w.setWord(kReadFrac, frac);
    const int32_t pos = w.word(kPos);
    const float length = w.real(kLength);
    const float into = mul(flt(vc2WalkMinus(pos, w.word(kRecStart))), w.real(kInverse));
    const float period = add(mul(into, sub(w.real(kNextLength), length)), length);
    const int32_t whole = xtf::ftrc(period);
    w.setWord(kPeriod, whole);
    int32_t to = pos;
    if (whole > pos) to = whole;
    else if (pos > vc2WalkMinus(w.word(kFinal), whole)) to = vc2WalkMinus(w.word(kFinal), whole);
    const bool ended = w.byte(kEnded) != 0;
    if (at > to) at = ended ? to : vc2WalkMinus(at, whole);
    if (to >= vc2WalkPlus(at, whole)) at = ended ? vc2WalkMinus(to, whole) : vc2WalkPlus(at, whole);
    w.setWord(kRead, at);
    float behind = div(flt(vc2WalkMinus(pos, at)), period);
    if (behind > k.fadeTop) behind = k.fadeTop;   // FCMP/GT both ways: a NaN stays
    if (k.fadeBottom > behind) behind = k.fadeBottom;
    return mul(k.fadeScale, sub(behind, k.fadeBottom));
}

// [FW] 0x893caa4e…: the two taps, the read and a period on, each a sample and its sixteenths (× 1/16); mono (half words,
// both sides the one) or stereo (left, then right); each tap between its sample and the next, the second crossfaded
// over the first; at the gain, the voice's
void taps(Vc2WalkWords& w, const Vc2OscSamples& wave, float fade, float* lr) {
    const Vc2OscJumpPool& k = vc2OscTables().jump;
    const uint32_t a = (uint32_t)w.word(kRead), b = (uint32_t)vc2WalkPlus(w.word(kRead), w.word(kPeriod));
    const float fa = mul(flt((int)(a & kLowBits)), k.sixteenth), fb = mul(flt((int)(b & kLowBits)), k.sixteenth);
    const bool stereo = (w.byte(kFlags) & kStereo) != 0;
    const uint32_t shift = stereo ? kStereoShift : kMonoShift, next = stereo ? kStereoNext : kMonoNext;
    const uint32_t from = (uint32_t)w.word(kSamples);
    const uint32_t at = from + ((a >> kSixteenths) << shift), bt = from + ((b >> kSixteenths) << shift);
    const auto side = [&](uint32_t off) {
        const float x = vc2OscBetween(wave.at(at + off), wave.at(at + off + next), fa);
        const float y = vc2OscBetween(wave.at(bt + off), wave.at(bt + off + next), fb);
        return add(mul(fade, sub(y, x)), x);
    };
    const float l = side(0);
    const float r = stereo ? side(kRight) : l;
    const float g = vc2OscGlide(w.v, kVoiceTarget, k.sampleScale);
    lr[0] = mul(l, g);
    lr[1] = mul(r, g);
}

// A frame: the walk's step, the read, the taps
void frame(Vc2WalkWords& w, const Vc2OscSamples& wave, float* lr) {
    step(w);
    const float fade = read(w);
    taps(w, wave, fade, lr);
}
} // namespace
} // namespace vc2jump

// [FW] 0x893ca8b0: a block on the walk (vc2WalkBlock) with the block's steps, each frame the walk's, the read's, the taps
void vc2OscJump(Vc2OscVoice& v, float* out, Vc2OscParts& p) {
    vc2WalkBlock(v, out, p, vc2jump::blockSteps, vc2jump::frame);
}
