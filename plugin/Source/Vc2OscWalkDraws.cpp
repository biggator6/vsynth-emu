// The draws of kind 1's mode 1 and the events' table it walks (v327): see Vc2OscDraws.h, oscDrawEventTable and
// oscDrawModeOneWords.  XT_VC2OSC_MAIN or XT_VC2TONE_MAIN only.
#if defined(XT_VC2OSC_MAIN) || defined(XT_VC2TONE_MAIN)
#include "Vc2OscDraws.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace {
// [FW] an event's entry: its flags' half word (its kind the flags & 0x0b: 1 a mark, 2 a return, 3 a turn; 0 and
// 8…11, an attack, the start, the end, the walk passes), its position 4 on
constexpr uint32_t kPosAfter = 4, kKindBits = 0x0b;
constexpr std::array<uint16_t, 6> kKinds { 1, 2, 3, 0, 9, 10 };
// The region about the table: below it the returns' search back, which steps by an event's own offset, and above it
// the marks' search forward, the same, each at most 12 events of 8 half words away
constexpr uint32_t kBelow = 0x800, kAbove = 0x1000;
constexpr int32_t kSixteen = 16;

template <typename T>
void intoTable(std::vector<uint8_t>& bytes, uint32_t at, T value) {
    std::memcpy(bytes.data() + at, &value, sizeof value);
}

// [FW] mode 1's object's words, from the voice's +0x15c: its flags (0x40 stereo, 0x01 the loops on), the events' and
// the walk's bits, the end, the current and next event's kinds, the events' first and stride, the current and last
// event's numbers, the table, the range's start and end, the last event's position, the current and next event's, the
// next crossing, the position and its fraction, the loops' record, the pitch, the rate factor and the loops', the
// samples' address
namespace ModeOne {
constexpr uint32_t kFlags = 0x0, kEventBits = 0x2, kWalkBits = 0x3, kEnded = 0x4, kKind = 0x5, kNextKind = 0x6;
constexpr uint32_t kFirst = 0x7, kWalk = 0x7c;   // [FW] and the beat's walk
constexpr uint8_t kBounceFwd = 0x08;
constexpr uint32_t kStride = 0x8, kIndex = 0xc, kLast = 0xe, kTable = 0x10, kStart = 0x18, kEnd = 0x1c;
constexpr uint32_t kFinal = 0x20, kPrev = 0x24, kNext = 0x28, kCross = 0x2c, kPos = 0x38, kFrac = 0x3c;
constexpr uint32_t kRecord = 0x6c, kPitch = 0x70, kRate = 0xa0, kFwdRate = 0xa4, kBackRate = 0xa8, kSamples = 0xec;
// [FW] the voice's: the gain's target, the gain and its rate
constexpr uint32_t kTarget = 0xdc, kGain = 0xe0, kGainRate = 0xe4;
} // namespace ModeOne

// A sample's step (byte 3's 0x01 alone, the pitch and rate 1, the fraction 0) from whole samples before the event the
// walk goes to (`ahead` forward, `behind` back), so that a frame lands on the crossing there
void steerLanding(Vc2OscVoice& o, std::mt19937& rng, int32_t ahead, int32_t behind) {
    using namespace ModeOne;
    constexpr uint8_t kOneSample = 0x01;   // [FW] the beat's tail's
    constexpr uint32_t kSamplesBefore = 20;
    const bool forward = o.f32(Vc2OscVoice::kObject + kWalk) > 0.0f;
    const int32_t whole = kSixteen * (1 + (int32_t)(rng() % kSamplesBefore));
    o.setU8(Vc2OscVoice::kObject + kWalkBits, kOneSample);
    for (uint32_t at : { kPitch, kRate }) o.setF32(Vc2OscVoice::kObject + at, 1.0f);
    o.setU32(Vc2OscVoice::kObject + kFrac, 0);
    o.setU32(Vc2OscVoice::kObject + kPos, (uint32_t)(forward ? ahead - whole : behind + whole));
}

// One of the record's four loop points (the cached events' positions, ×16: the forward loop's start and end, the
// backward's end and start), as the records' draw left it
int32_t loopPoint(const OscRecords& r, uint32_t record, size_t which) {
    constexpr std::array<uint32_t, 4> kPoints { 0x4, 0xb8, 0x16c, 0x220 };   // [FW]
    const auto at = r.bound.find(record + kPoints[which]);
    return at == r.bound.end() ? 0 : std::bit_cast<int32_t>(at->second);
}

// What the walk's steers are given: the voice, the records, and the events the walk goes to and comes from
struct WalkSteer {
    Vc2OscVoice& o;
    std::mt19937& rng;
    const OscRecords& r;
    int32_t ahead, behind;
};

// Bouncing as an Alternate loop leaves it, the walk forward (the beat's tail keeps the bounce's bit so): half the time
// in the forward loop (byte 3 0x08 alone, the step going back), at a mark (+0x5) with byte 2's 0x01, where a mark's
// search back meets a turn bouncing; else in the backward loop (0x04, the step going forward).  The event it goes to
// at the loop's last point (back the backward loop's start, forward the forward loop's end), the position whole
// samples before it, where the crossing meets the point while bouncing
void steerBounce(WalkSteer& s) {
    using namespace ModeOne;
    constexpr uint8_t kMark = 1, kActs = 0x01, kBounceBack = 0x04;   // [FW]
    constexpr size_t kBackStart = 3, kFwdEnd = 1;                     // the record's loop points
    constexpr uint32_t kSamplesBefore = 20;
    const bool back = s.rng() % 2 != 0;
    const int32_t point = loopPoint(s.r, s.o.u32(Vc2OscVoice::kObject + kRecord), back ? kBackStart : kFwdEnd);
    const int32_t whole = kSixteen * (1 + (int32_t)(s.rng() % kSamplesBefore));
    s.o.setU8(Vc2OscVoice::kObject + kWalkBits, back ? kBounceFwd : kBounceBack);
    s.o.setF32(Vc2OscVoice::kObject + kWalk, std::fabs(s.o.f32(Vc2OscVoice::kObject + kWalk)));
    s.o.setU32(Vc2OscVoice::kObject + (back ? kPrev : kNext), (uint32_t)point);
    s.o.setU32(Vc2OscVoice::kObject + kPos, (uint32_t)(back ? point + whole : point - whole));
    if (!back) return;
    s.o.setU8(Vc2OscVoice::kObject + kKind, kMark);
    s.o.setU8(Vc2OscVoice::kObject + kEventBits, kActs);
}
// A sample's step onto the event ahead
void steerToEvent(WalkSteer& s) { steerLanding(s.o, s.rng, s.ahead, s.behind); }
// A loop point ahead of the walk, the near one (forward the forward loop's end, back the backward's start) or the far
// (the backward loop's end, the forward's start): a sample's step onto it, or the position on it with the walk's
// in-loop bits set, where the next crossing finds it at the position
void steerAtPoint(WalkSteer& s) {
    using namespace ModeOne;
    constexpr uint8_t kInLoops = 0x31;   // [FW] in both loops, a sample's step
    const bool forward = s.o.f32(Vc2OscVoice::kObject + kWalk) > 0.0f;
    const bool near = s.rng() % 2 != 0;
    const size_t which = forward ? (near ? 1 : 2) : (near ? 3 : 0);
    const int32_t point = loopPoint(s.r, s.o.u32(Vc2OscVoice::kObject + kRecord), which);
    if (s.rng() % 2) {
        steerLanding(s.o, s.rng, point, point);
        return;
    }
    s.o.setU8(Vc2OscVoice::kObject + kWalkBits, kInLoops);
    s.o.setU32(Vc2OscVoice::kObject + kPos, (uint32_t)point);
}
void steerNone(WalkSteer&) {}

// The walk's steers, one time in four each (the fourth none)
void steerWalk(Vc2OscVoice& o, std::mt19937& rng, const OscRecords& r, int32_t ahead, int32_t behind) {
    constexpr std::array<void (*)(WalkSteer&), 4> kSteers { steerBounce, steerToEvent, steerAtPoint, steerNone };
    WalkSteer s { o, rng, r, ahead, behind };
    kSteers[rng() % kSteers.size()](s);
}
} // namespace

void oscDrawEventTable(Vc2Fw& fw, OscShared& shared) {
    std::mt19937& rng = shared.renders;
    OscEventTable& t = shared.records.events;
    t.count = 4 + (int)(rng() % 9);
    // [FW] even, as the XT's must be: an entry's position is a MOV.L, an address error unless aligned
    t.stride = (uint8_t)(4 + 2 * (rng() % 3));
    t.first = (uint8_t)(2 * (rng() % 2));
    t.from = kOscEventsAt - kBelow;
    t.bytes.assign(kBelow + kAbove, 0);
    t.positions.assign((size_t)t.count, 0);
    constexpr int32_t kFirstFrame = 2500, kFrames = 2400;
    const int32_t gap = kFrames / t.count;
    int32_t frame = kFirstFrame;
    for (int i = 0; i < t.count; ++i) {
        frame += 1 + (int32_t)(rng() % (uint32_t)gap);
        t.positions[(size_t)i] = kSixteen * frame + (int32_t)(rng() % kSixteen);
        const uint32_t at = kBelow + 2 * ((uint32_t)i * t.stride + t.first);
        uint16_t kind = rng() % 4 ? kKinds[rng() % 3] : kKinds[3 + rng() % 3];
        if (i == 0 && rng() % 2) kind = kKinds[0];   // the first a mark half the time, where a return's search ends
        intoTable(t.bytes, at, (uint16_t)((rng() & ~kKindBits) | kind));
        intoTable(t.bytes, at + kPosAfter, t.positions[(size_t)i]);
    }
    fw.cpu.write(t.from, t.bytes.data(), t.bytes.size());
}

void oscDrawModeOneWords(Vc2OscVoice& o, std::mt19937& rng, const OscRecords& r, uint32_t waveAt) {
    using namespace ModeOne;
    const OscEventTable& t = r.events;
    const auto put = [&](uint32_t at, int32_t word) { o.setU32(Vc2OscVoice::kObject + at, (uint32_t)word); };
    const auto putByte = [&](uint32_t at, uint32_t b) { o.setU8(Vc2OscVoice::kObject + at, (uint8_t)b); };
    const auto putReal = [&](uint32_t at, float v) { o.setF32(Vc2OscVoice::kObject + at, v); };
    putByte(kFlags, rng());
    oscDrawBeatWords(o, rng);
    // byte 2 0 one time in four, as a note leaves it; else any, with 0x01 (returns and turns act) three times in four
    // and 0x02 (the next passes) one time in four, so that returns and turns act
    constexpr uint32_t kActs = 0x01, kPasses = 0x02;
    const uint32_t events = rng() & ~(kActs | kPasses);
    const uint32_t acts = rng() % 4 ? kActs : 0;
    const uint32_t passes = rng() % 4 == 0 ? kPasses : 0;
    putByte(kEventBits, rng() % 4 ? events | acts | passes : 0);
    constexpr std::array<uint8_t, 4> kEnds { 0, 0, 1, 0xff };   // [FW] looping, or past the last event or the first
    putByte(kEnded, kEnds[rng() % kEnds.size()]);
    const int last = rng() % 2 ? t.count - 1 : (int)(rng() % (uint32_t)t.count);
    const int index = (int)(rng() % (uint32_t)(last + 1));
    o.setU16(Vc2OscVoice::kObject + kIndex, (uint16_t)index);
    o.setU16(Vc2OscVoice::kObject + kLast, (uint16_t)last);
    putByte(kFirst, t.first);
    putByte(kStride, t.stride);
    put(kTable, (int32_t)kOscEventsAt);
    const auto event = [&](int n) { return n <= last ? t.positions[(size_t)n] : t.positions[(size_t)last] + 16; };
    put(kFinal, t.positions[(size_t)last]);
    put(kPrev, event(index));
    put(kNext, event(index + 1));
    putByte(kKind, rng() & kKindBits);
    putByte(kNextKind, rng() & kKindBits);
    put(kCross, (int32_t)rng());
    const int32_t start = t.positions.front() - kSixteen * (int32_t)(rng() % 200);
    const int32_t end = t.positions[(size_t)last] + kSixteen * (int32_t)(rng() % 200);
    put(kStart, start);
    put(kEnd, end);
    const int32_t near = std::clamp(event(index) + (int32_t)(rng() % 4096) - 2048, start, end);
    put(kPos, rng() % 8 ? near : (int32_t)(rng() % (uint32_t)end));
    put(kFrac, (int32_t)(rng() & 0xfffff));
    put(kRecord, (int32_t)oscPairAt((int)(rng() % kOscRecords)));
    for (uint32_t at : { kPitch, kRate, kFwdRate, kBackRate }) putReal(at, oscWithin(rng, 0.5f, 2.0f));
    put(kSamples, (int32_t)waveAt);
    for (uint32_t at : { kTarget, kGain }) o.setF32(at, oscWithin(rng, 0.0f, 0.5f));
    o.setF32(kGainRate, oscWithin(rng, 0.0f, 0.125f));
    steerWalk(o, rng, r, event(index + 1), event(index));
}
#endif
