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
// the marks' search forward, the same, each at most 12 events of (from v328) 88 half words away
constexpr uint32_t kBelow = 0x8000, kAbove = 0x8000;
// [FW] v328: the layouts mode 1's set-up gives the events by the header's format and the samples' frame, the first
// half word and the strides mono and stereo; the table's count of events, a half word at +0xa
struct EventLayout {
    uint8_t first, mono, stereo;
};
constexpr EventLayout kNarrowEvents { 8, 10, 16 }, kWideEvents { 14, 46, 88 };
constexpr uint32_t kCountAt = 0xa;
constexpr int32_t kSixteen = 16;

template <typename T>
void intoTable(std::vector<uint8_t>& bytes, uint32_t at, T value) {
    std::memcpy(bytes.data() + at, &value, sizeof value);
}
// An entry of the table at `at`: its half word of `kind` (the other bits any) and its position
void entryInto(OscEventTable& t, uint32_t at, uint16_t kind, int32_t pos, std::mt19937& rng) {
    intoTable(t.bytes, at, (uint16_t)((rng() & ~kKindBits) | kind));
    intoTable(t.bytes, at + kPosAfter, pos);
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
    // v328: the mark closed one time in four, where the walk back un-bounces (v327 reached it by chance alone)
    s.o.setU8(Vc2OscVoice::kObject + kEventBits, s.rng() % 4 != 0 ? kActs : 0);
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
    t.wide = rng() % 2 != 0;
    t.stereo = rng() % 2 != 0;
    const EventLayout& layout = t.wide ? kWideEvents : kNarrowEvents;
    t.stride = t.stereo ? layout.stereo : layout.mono;
    t.first = layout.first;
    t.from = kOscEventsAt - kBelow;
    t.bytes.assign(kBelow + kAbove, 0);
    intoTable(t.bytes, kBelow + kCountAt, (uint16_t)t.count);
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
        entryInto(t, at, kind, t.positions[(size_t)i], rng);
    }
    // v328: the header's first half word a mark one case in two, and the entry past the last event a return or a turn,
    // each at an event's position: from first half words of 8 or 14, a return's search back meets the table's start
    // and a mark's forward search the entry past the last, which held zeros
    const auto position = [&] { return t.positions[rng() % t.positions.size()]; };
    if (rng() % 2) entryInto(t, kBelow, kKinds[0], position(), rng);
    const uint32_t past = kBelow + 2 * ((uint32_t)t.count * t.stride + t.first);
    if (rng() % 2) entryInto(t, past, kKinds[1 + rng() % 2], position(), rng);
    fw.cpu.write(t.from, t.bytes.data(), t.bytes.size());
}

namespace {
// [FW] v328: the voice's words mode 1's set-up reads through the head, as a note-on's block gives them (the head the
// voice's own +0x108): the events' table, the samples, a header, a loop record, a +0xe0 record; the head; LITE's kind
// and mode 1's type
struct RecordWord {
    uint32_t at, word;
};
constexpr uint32_t kHeadAt = 0x1a0, kHeadOf = 0x108, kLite = 1, kModeOneType = 1;
constexpr uint32_t kTypeWord = (Vc2OscVoice::kWave - Vc2OscVoice::kOwn) / 4;   // a note-on's block's
std::array<RecordWord, 5> modeOneRecords(std::mt19937& rng) {
    const int k = (int)(rng() % kOscRecords);
    return { { { 0x108, kOscEventsAt }, { 0x10c, kOscWaveAt }, { 0x128, oscHeaderAt(k) }, { 0x130, oscLoopAt(k) },
               { 0x134, oscPairAt(k) } } };
}
// byte 0's frame (0x40 stereo: the table's, as a wave's table follows its own frame; the other's stride reads across
// entries, whose positions then leave the wave, where rom.4 reads memory the port doesn't have) and the range in
// samples (0x80) half the time
void steerFrame(Vc2OscVoice& o, std::mt19937& rng, bool stereo) {
    constexpr uint8_t kStereo = 0x40, kInSamples = 0x80;
    const uint8_t frame = (uint8_t)((stereo ? kStereo : 0) | (rng() % 2 != 0 ? kInSamples : 0));
    o.setU8(Vc2OscVoice::kObject, (uint8_t)((o.u8(Vc2OscVoice::kObject) & ~(kStereo | kInSamples)) | frame));
}
// the range's events (past the last now and then, which the start holds to it) and samples about the table's
void steerRange(Vc2OscVoice& o, std::mt19937& rng, const OscEventTable& t) {
    constexpr uint32_t kRangeFirst = 0x14, kRangeLast = 0x16, kRangeStart = 0x18, kRangeEnd = 0x1c;   // [FW]
    for (uint32_t at : { kRangeFirst, kRangeLast }) {   // one time in eight 0x8000 up: the start's hold is unsigned
        const uint32_t n = rng() % (t.count + 2);
        o.setU16(Vc2OscVoice::kObject + at, (uint16_t)(rng() % 8 == 0 ? 0x8000 | n : n));
    }
    for (uint32_t at : { kRangeStart, kRangeEnd }) {
        const int32_t near = t.positions[rng() % t.positions.size()] + (int32_t)(rng() % 1024) - 512;
        o.setU32(Vc2OscVoice::kObject + at, (uint32_t)near);
    }
}
} // namespace

void oscSteerModeOneSetUp(Vc2OscVoice& o, OscShared& shared, uint32_t voiceAt) {
    // [FW] the head's tempo words, the wave's Original Tempo and the note's (each ×10000), which the beat's start
    // divides by: 40…250 BPM, as a note-on's head writes them (at 0 the start's factor is 0/0)
    constexpr std::array<uint32_t, 2> kTempos { 0x1a4, 0x1a8 };
    constexpr uint32_t kLeast = 400000, kSpan = 2100000;
    std::mt19937& rng = shared.renders;
    for (const auto& [at, word] : modeOneRecords(rng)) o.setU32(at, word);
    o.setU32(kHeadAt, voiceAt + kHeadOf);
    for (uint32_t at : kTempos) o.setU32(at, kLeast + (uint32_t)(rng() % kSpan));
    steerFrame(o, rng, shared.records.events.stereo);
    steerRange(o, rng, shared.records.events);
    o.setF32(Vc2OscVoice::kObject + ModeOne::kWalk, (rng() % 2 ? 1.0f : -1.0f) * oscWithin(rng, 0.1f, 3.0f));
}

bool oscIsModeOne(const Vc2OscVoice& o) { return o.kind() == kLite && o.u32(Vc2OscVoice::kWave) == kModeOneType; }

bool oscBlockIsModeOne(const uint32_t* block) { return block[0] == kLite && block[kTypeWord] == kModeOneType; }

void oscModeOneBlock(uint32_t* block, std::mt19937& rng) {
    if (!oscBlockIsModeOne(block)) return;
    for (const auto& [at, word] : modeOneRecords(rng)) block[(at - Vc2OscVoice::kOwn) / 4] = word;
}

long oscSteerModeOneSeek(Vc2OscVoice& o, OscShared& shared, bool seeks) {
    if (!seeks) return 0;
    oscDrawModeOneWords(o, shared.renders, shared.records, kOscWaveAt);
    return 1;
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
