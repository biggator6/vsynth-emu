// A band wave's parts for the band players' checks (v332, Vc2OscBandDraws.cpp's): its vpdt, laid out as the players
// read it, its loop and cached loop events, and the room it takes.  XT_VC2OSC_MAIN or XT_VC2TONE_MAIN only.
#if defined(XT_VC2OSC_MAIN) || defined(XT_VC2TONE_MAIN)
#include "Vc2OscDraws.h"
#include "XtMultiBandPlayer.h"
#include <algorithm>
#include <cstring>

namespace {
// [FW] the vpdt: the events' count in +8's high half; the events from +0x1c, each `stride` bytes: its kind's half word,
// a half word, its position (×16) at +4, then the bands' slots from +8 (band 0, an empty one, bands 1…5) of 12 bytes
// mono and 24 stereo: its segment entry's offset, its record's and the mark to the record's end
constexpr uint32_t kCountAt = 8, kFirstAt = 0x1c, kPosAt = 4, kSlotsAt = 8, kCountShift = 16;
constexpr uint32_t kMonoStride = 0x2e * 2, kStereoStride = 0x58 * 2, kMonoSlot = 12, kStereoSlot = 24;
// [FW] a segment entry: its records' offset (bit 0 a flag), a word, its start; a record: its length and its info, whose
// bits 0x1f8 are the repeat budget (<< 6), all set always repeating: so a record past the wave's end, where a walk that
// stops there leaves the bands repeating it (one that moved on would read the next band's tables as records, and
// past the memory)
constexpr uint32_t kEntryBytes = 12, kRecordBytes = 4, kRecordInfoAt = 2;
constexpr uint16_t kRepeats = 0x1f8;
constexpr int kBands = 6;
constexpr int32_t kSixteen = 16, kNever = 0x7fffffff;
// [FW] the cached loop events' position in a record; the two no loop reaches, the forward loop's end and the backward's
constexpr uint32_t kCachedPos = 4;
constexpr std::array<uint32_t, 2> kUnreached { 1, 2 };
// [FW] the events' kinds (their half word's 0x0b), the walk acting on 1…3, mostly none
constexpr std::array<uint16_t, 8> kKinds { 0, 0, 0, 0, 1, 2, 3, 9 };
constexpr uint16_t kKindBits = 0x0b;
static_assert(kOscBandCachedBytes == 4 * kXtMbLoopEventBytes);

struct BandBytes {
    std::vector<uint8_t> bytes;
    template <typename T> void at(uint32_t to, T v) { std::memcpy(bytes.data() + to, &v, sizeof v); }
    uint32_t grow(uint32_t n) {
        const uint32_t to = (uint32_t)bytes.size();
        bytes.resize(to + n);
        return to;
    }
};
struct BandRecordAt {
    int32_t start, length;
    uint32_t at;
};
struct BandSegmentAt {
    int32_t start;
    uint32_t entry;
    std::vector<BandRecordAt> records;
};

int32_t sixteenths(int frames, std::mt19937& rng) { return kSixteen * (1 + (int32_t)(rng() % (uint32_t)(frames - 1))); }

// Positions (×16): the first at 0, `more` inside the wave, sorted, and (`last`) one at its end
std::vector<int32_t> bandPositions(int frames, int more, bool last, std::mt19937& rng) {
    std::vector<int32_t> p { 0 };
    for (int i = 0; i < more; ++i) p.push_back(sixteenths(frames, rng));
    std::sort(p.begin(), p.end());
    if (last) p.push_back(kSixteen * frames);
    return p;
}

// A band's segments, each its records from its start past the next's (the last's past the wave's end), laid out: the
// entries, a last one never reached, then each segment's records and four short ones for the last entry.  Band k's
// records are 16…215 samples × 2^min(k, 4), twice its read's step at the pitch's most at least, as an encoder's grow
// with its band's decimation: a step past a whole record makes the repeat at an end wrap its position to near 2^31
// (0x8950f50a's), and rom.4's reads leave its memory
std::vector<BandSegmentAt> bandSegments(BandBytes& v, int frames, int k, std::mt19937& rng) {
    const std::vector<int32_t> starts = bandPositions(frames, (int)(rng() % 3), false, rng);
    std::vector<BandSegmentAt> segs;
    const uint32_t entries = v.grow(kEntryBytes * (uint32_t)(starts.size() + 1));
    for (size_t i = 0; i < starts.size(); ++i) {
        const int32_t end = i + 1 < starts.size() ? starts[i + 1] : kSixteen * (frames + kOscBandTail);
        BandSegmentAt s { starts[i], entries + kEntryBytes * (uint32_t)i, {} };
        for (int32_t at = s.start; at < end;) {
            const int32_t length = kSixteen * (16 + (int32_t)(rng() % 200)) << std::min(k, 4);
            s.records.push_back({ at, length, v.grow(kRecordBytes) });
            at += length;
        }
        v.at(s.entry, s.records.front().at | (rng() & 1));
        v.at(s.entry + 4, (uint32_t)rng());
        v.at(s.entry + 8, s.start);
        segs.push_back(s);
    }
    const uint32_t last = entries + kEntryBytes * (uint32_t)starts.size(), ends = v.grow(4 * kRecordBytes);
    for (uint32_t r = 0; r < 4; ++r) {
        v.at(ends + kRecordBytes * r, (uint16_t)(kSixteen * kSixteen));
        v.at(ends + kRecordBytes * r + kRecordInfoAt, kRepeats);
    }
    v.at(last, ends);
    v.at(last + 8, kNever);
    for (const BandSegmentAt& s : segs) {
        for (const BandRecordAt& r : s.records) {
            const bool past = r.start + r.length > kSixteen * frames;
            v.at(r.at, (uint16_t)r.length);
            v.at(r.at + kRecordInfoAt, (uint16_t)(past ? (rng() & ~(uint32_t)kRepeats) | kRepeats : rng()));
        }
    }
    return segs;
}

// Event `p`'s slot of a band: its segment's entry, the record holding it and the mark to that record's end
void bandSlot(BandBytes& v, uint32_t slot, const std::vector<BandSegmentAt>& segs, int32_t p) {
    const auto seg = std::find_if(segs.rbegin(), segs.rend(), [p](const BandSegmentAt& s) { return s.start <= p; });
    const auto rec = std::find_if(seg->records.begin(), seg->records.end(),
                                  [p](const BandRecordAt& r) { return p < r.start + r.length; });
    v.at(slot, seg->entry);
    v.at(slot + 4, rec->at);
    v.at(slot + 8, (uint16_t)(rec->start + rec->length - p));
}

uint32_t aligned(uint32_t at) { return (at + 3) & ~3u; }
} // namespace

// The header, the events (kinds and positions), each band's segments and records, then each event's slots
std::vector<uint8_t> oscBandVpdt(int frames, int events, bool stereo, std::mt19937& rng) {
    const uint32_t stride = stereo ? kStereoStride : kMonoStride, slot = stereo ? kStereoSlot : kMonoSlot;
    BandBytes v;
    v.grow(kFirstAt + stride * (uint32_t)events);
    for (uint32_t i = kFirstAt; i < v.bytes.size(); ++i) v.bytes[i] = (uint8_t)rng();
    v.at(kCountAt, (uint32_t)events << kCountShift);
    const std::vector<int32_t> p = bandPositions(frames, events - 2, true, rng);
    for (int e = 0; e < events; ++e) {
        const uint32_t entry = kFirstAt + stride * (uint32_t)e;
        v.at(entry, (uint16_t)((rng() & ~(uint32_t)kKindBits) | kKinds[rng() % kKinds.size()]));
        v.at(entry + kPosAt, p[(size_t)e]);
    }
    for (int k = 0; k < kBands; ++k) {
        const std::vector<BandSegmentAt> segs = bandSegments(v, frames, k, rng);
        const uint32_t index = k == 0 ? 0 : (uint32_t)k + 1;   // [FW] band 0, the empty slot, bands 1…5
        for (int e = 0; e < events; ++e) {
            bandSlot(v, kFirstAt + stride * (uint32_t)e + kSlotsAt + slot * index, segs, p[(size_t)e]);
        }
    }
    return v.bytes;
}

// On two times in three: its points within the wave, a loop at least 64 frames, its modes any
XtLoop oscBandLoop(int frames, std::mt19937& rng) {
    XtLoop l;
    l.on = rng() % 3 != 0;
    const auto span = [&](int& from, int& to) {
        from = (int)(rng() % (uint32_t)(frames / 2));
        to = from + kOscBandTail + (int)(rng() % (uint32_t)(frames - from - kOscBandTail));
    };
    span(l.fwdStart, l.fwdEnd);
    span(l.bwdStart, l.bwdEnd);
    l.fwdMode = (int)(rng() % 4);
    l.bwdMode = (int)(rng() % 4);
    return l;
}

// By xtMbLoopEvents; with no loop, zeros but the two positions no walk reaches
void oscBandCachedEvents(uint8_t* to, const std::vector<uint8_t>& vpdt, bool stereo, const XtLoop& loop) {
    const XtMbWave w { .vpdt = vpdt.data(), .vpdtBytes = vpdt.size(), .channels = stereo ? 2 : 1 };
    uint8_t ev[4][kXtMbLoopEventBytes] {};
    if (!loop.on || !xtMbLoopEvents(w, loop, ev)) {
        std::memset(ev, 0, sizeof ev);
        for (uint32_t r : kUnreached) std::memcpy(ev[r] + kCachedPos, &kNever, sizeof kNever);
    }
    std::memcpy(to, ev, sizeof ev);
}

// Band 0 the wave's frames and a tail, each other a quarter, an eighth… of them and a tail
uint32_t oscBandFrames(int k, int frames) {
    return (uint32_t)((frames + kOscBandTail) >> (k == 0 ? 0 : k + 1)) + kOscBandTail;
}

uint32_t oscBandRoom(int frames, bool stereo, uint32_t vpdtBytes) {
    uint32_t n = aligned(vpdtBytes) + kOscBandCachedBytes + 2 * kOscBandRecordRoom;
    for (int k = 0; k < kBands; ++k) n += aligned(oscBandFrames(k, frames) * (stereo ? 4 : 2)) + kOscBandTail;
    return n;
}
#endif
