// A band wave's parts for the band players' checks (v332, Vc2OscBandDraws.cpp's): its vpdt, laid out as the players
// read it, its loop and cached loop events, and the room it takes.  From v334 a SOLO wave's too: the same vpdt in
// SOLO's layout (its events from +0x10, 10 or 16 half words apart, one band whose slot is the event's +8, now and then
// a record of two entries), its cached loop events by its segments.  XT_VC2OSC_MAIN or XT_VC2TONE_MAIN only.
#if defined(XT_VC2OSC_MAIN) || defined(XT_VC2TONE_MAIN)
#include "Vc2OscDraws.h"
#include "XtMultiBandPlayer.h"
#include <algorithm>
#include <cstring>

namespace {
// [FW] the vpdt: the events' count in +8's high half; the events from their first byte, each `stride` bytes: its kind's
// half word, a half word, its position (×16) at +4, then the bands' slots from +8 (band 0, an empty one, bands 1…5)
// of 12 bytes mono and 24 stereo: its segment entry's offset, its record's and the mark to the record's end
constexpr uint32_t kCountAt = 8, kPosAt = 4, kSlotsAt = 8, kCountShift = 16;
// [FW] the band players' layout (from +0x1c, 0x2e and 0x58 half words apart, six bands) and SOLO's (v334)
struct OscVpdtLayout {
    uint32_t first, mono, stereo;
    int bands;
    bool soloRecords;   // now and then a record of two entries, one under the period's floor, one of no budget
};
constexpr OscVpdtLayout kBandVpdt { 0x1c, 0x2e * 2, 0x58 * 2, 6, false }, kSoloVpdt { 0x10, 10 * 2, 16 * 2, 1, true };
constexpr uint32_t kMonoSlot = 12, kStereoSlot = 24;
const OscVpdtLayout& layoutOf(bool solo) { return solo ? kSoloVpdt : kBandVpdt; }
// [FW] a segment entry: its records' offset (bit 0 a flag), a word, its start; a record: its length and its info, whose
// bits 0x1f8 are the repeat budget (<< 6), all set always repeating: so a record past the wave's end, where a walk that
// stops there leaves the bands repeating it (one that moved on would read the next band's tables as records, and
// past the memory).  A length of 0xffff goes on into the next entry (SOLO's records of two entries, v334)
constexpr uint32_t kEntryBytes = 12, kRecordBytes = 4, kRecordInfoAt = 2;
constexpr uint16_t kRepeats = 0x1f8, kOnward = 0xffff;
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

int32_t sixteenths(int frames, std::mt19937& rng) { return kSixteen * (1 + (int32_t)(rng() % (uint32_t)(frames - 1))); }

// Positions (×16): the first at 0, `more` inside the wave, sorted, and (`last`) one at its end
std::vector<int32_t> bandPositions(int frames, int more, bool last, std::mt19937& rng) {
    std::vector<int32_t> p { 0 };
    for (int i = 0; i < more; ++i) p.push_back(sixteenths(frames, rng));
    std::sort(p.begin(), p.end());
    if (last) p.push_back(kSixteen * frames);
    return p;
}

// A record's length: 16…215 samples × 2^min(k, 4); v334's SOLO one in 16 in two entries (0xffff on more) and one in
// 16 of 4…15 samples, under the grain parameters' floor of 10
int32_t recordLength(const OscVpdtLayout& l, int k, std::mt19937& rng) {
    const int32_t length = kSixteen * (16 + (int32_t)(rng() % 200)) << std::min(k, 4);
    if (!l.soloRecords) return length;
    const uint32_t pick = rng() % 16;
    if (pick == 0) return length + kOnward;
    return pick == 1 ? kSixteen * (4 + (int32_t)(rng() % 12)) : length;
}
uint16_t firstLength(const OscBandRecord& r) { return (uint16_t)std::min(r.length, (int32_t)kOnward); }

// A band's segments, each its records from its start past the next's (the last's past the wave's end), laid out: the
// entries, a last one never reached, then each segment's records and four short ones for the last entry.  Band k's
// records are 16…215 samples × 2^min(k, 4), twice its read's step at the pitch's most at least, as an encoder's grow
// with its band's decimation: a step past a whole record makes the repeat at an end wrap its position to near 2^31
// (0x8950f50a's), and rom.4's reads leave its memory
std::vector<OscBandSegment> bandSegments(BandBytes& v, const OscVpdtLayout& l, int frames, int k, std::mt19937& rng) {
    const std::vector<int32_t> starts = bandPositions(frames, (int)(rng() % 3), false, rng);
    std::vector<OscBandSegment> segs;
    const uint32_t entries = v.grow(kEntryBytes * (uint32_t)(starts.size() + 1));
    for (size_t i = 0; i < starts.size(); ++i) {
        const int32_t end = i + 1 < starts.size() ? starts[i + 1] : kSixteen * (frames + kOscBandTail);
        OscBandSegment s { starts[i], entries + kEntryBytes * (uint32_t)i, {} };
        for (int32_t at = s.start; at < end;) {
            const int32_t length = recordLength(l, k, rng);
            s.records.push_back({ at, length, v.grow(length > kOnward ? 2 * kRecordBytes : kRecordBytes) });
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
    for (const OscBandSegment& s : segs) {
        for (const OscBandRecord& r : s.records) {
            const bool past = r.start + r.length > kSixteen * frames;
            uint32_t info = past ? (rng() & ~(uint32_t)kRepeats) | kRepeats : rng();
            if (l.soloRecords && !past && rng() % 8 == 0) info &= ~(uint32_t)kRepeats;   // v334: no budget
            const uint32_t at = r.length > kOnward ? r.at + kRecordBytes : r.at;
            if (r.length > kOnward) v.at(r.at, kOnward);
            if (r.length > kOnward) v.at(r.at + kRecordInfoAt, (uint16_t)rng());
            v.at(at, (uint16_t)(r.length > kOnward ? r.length - kOnward : r.length));
            v.at(at + kRecordInfoAt, (uint16_t)info);
        }
    }
    return segs;
}

// Position `p`'s slot of a band: its segment's entry, the record holding it and its mark (the record's start + its
// first length − p: a one-entry record's end − p)
void bandSlot(uint8_t* to, const std::vector<OscBandSegment>& segs, int32_t p) {
    const auto seg = std::find_if(segs.rbegin(), segs.rend(), [p](const OscBandSegment& s) { return s.start <= p; });
    const auto rec = std::find_if(seg->records.begin(), seg->records.end(),
                                  [p](const OscBandRecord& r) { return p < r.start + r.length; });
    std::memcpy(to, &seg->entry, sizeof seg->entry);
    std::memcpy(to + 4, &rec->at, sizeof rec->at);
    const uint16_t mark = (uint16_t)(rec->start + firstLength(*rec) - p);
    std::memcpy(to + 8, &mark, sizeof mark);
}

uint32_t aligned(uint32_t at) { return (at + 3) & ~3u; }
} // namespace

// The header, the events (kinds and positions), each band's segments and records, then each event's slots; band 0's
// segments into `zero` when given
std::vector<uint8_t> oscBandVpdt(int frames, int events, bool stereo, std::mt19937& rng, bool solo,
                                 std::vector<OscBandSegment>* zero) {
    const OscVpdtLayout& l = layoutOf(solo);
    const uint32_t stride = stereo ? l.stereo : l.mono, slot = stereo ? kStereoSlot : kMonoSlot;
    BandBytes v;
    v.grow(l.first + stride * (uint32_t)events);
    for (uint32_t i = l.first; i < v.bytes.size(); ++i) v.bytes[i] = (uint8_t)rng();
    v.at(kCountAt, (uint32_t)events << kCountShift);
    const std::vector<int32_t> p = bandPositions(frames, events - 2, true, rng);
    for (int e = 0; e < events; ++e) {
        const uint32_t entry = l.first + stride * (uint32_t)e;
        v.at(entry, (uint16_t)((rng() & ~(uint32_t)kKindBits) | kKinds[rng() % kKinds.size()]));
        v.at(entry + kPosAt, p[(size_t)e]);
    }
    for (int k = 0; k < l.bands; ++k) {
        const std::vector<OscBandSegment> segs = bandSegments(v, l, frames, k, rng);
        const uint32_t index = k == 0 ? 0 : (uint32_t)k + 1;   // [FW] band 0, the empty slot, bands 1…5
        for (int e = 0; e < events; ++e) {
            const uint32_t at = l.first + stride * (uint32_t)e + kSlotsAt + slot * index;
            bandSlot(v.bytes.data() + at, segs, p[(size_t)e]);
        }
        if (k == 0 && zero != nullptr) *zero = segs;
    }
    return v.bytes;
}

// v333: the events' positions as the vpdt holds them
std::vector<int32_t> oscBandPositions(const std::vector<uint8_t>& vpdt, bool stereo, bool solo) {
    const OscVpdtLayout& l = layoutOf(solo);
    const uint32_t stride = stereo ? l.stereo : l.mono;
    uint32_t count = 0;
    std::memcpy(&count, vpdt.data() + kCountAt, sizeof count);
    std::vector<int32_t> p(count >> kCountShift);
    for (uint32_t e = 0; e < p.size(); ++e) std::memcpy(&p[e], vpdt.data() + l.first + stride * e + kPosAt, 4);
    return p;
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

// With no loop, zeros but the two positions no walk reaches
void noLoopEvents(uint8_t* to) {
    std::memset(to, 0, kOscBandCachedBytes);
    for (uint32_t r : kUnreached) std::memcpy(to + r * kXtMbLoopEventBytes + kCachedPos, &kNever, sizeof kNever);
}

// By xtMbLoopEvents
void oscBandCachedEvents(uint8_t* to, const std::vector<uint8_t>& vpdt, bool stereo, const XtLoop& loop) {
    const XtMbWave w { .vpdt = vpdt.data(), .vpdtBytes = vpdt.size(), .channels = stereo ? 2 : 1 };
    uint8_t ev[4][kXtMbLoopEventBytes] {};
    if (!loop.on || !xtMbLoopEvents(w, loop, ev)) return noLoopEvents(to);
    std::memcpy(to, ev, sizeof ev);
}

// v334: SOLO's, as xtMbLoopEvents lays out band 0's: each point's last event before it and its loop's mode (the
// forward end's and the backward start's), its position ×16, its slot by band 0's segments
void oscSoloCachedEvents(uint8_t* to, const std::vector<int32_t>& events, const std::vector<OscBandSegment>& zero,
                         const XtLoop& loop) {
    if (!loop.on) return noLoopEvents(to);
    std::memset(to, 0, kOscBandCachedBytes);
    const std::array<int, 4> points { loop.fwdStart, loop.fwdEnd, loop.bwdEnd, loop.bwdStart };
    const std::array<int, 4> modes { 0, loop.fwdMode, 0, loop.bwdMode };
    for (uint32_t r = 0; r < points.size(); ++r) {
        uint8_t* o = to + r * kXtMbLoopEventBytes;
        const int32_t p = kSixteen * points[r];
        const auto after = std::ranges::lower_bound(events, p);   // the first at or past it: event 0 at the least
        const uint16_t before = (uint16_t)std::max<std::ptrdiff_t>(after - events.begin() - 1, 0);
        const uint16_t mode = (uint16_t)modes[r];
        std::memcpy(o, &before, sizeof before);
        std::memcpy(o + 2, &mode, sizeof mode);
        std::memcpy(o + kCachedPos, &p, sizeof p);
        bandSlot(o + kSlotsAt, zero, p);
    }
}

// Band 0 the wave's frames and a tail, each other a quarter, an eighth… of them and a tail
uint32_t oscBandFrames(int k, int frames) {
    return (uint32_t)((frames + kOscBandTail) >> (k == 0 ? 0 : k + 1)) + kOscBandTail;
}

uint32_t oscBandRoom(int frames, bool stereo, uint32_t vpdtBytes, bool solo) {
    uint32_t n = aligned(vpdtBytes) + kOscBandCachedBytes + 2 * kOscBandRecordRoom;
    for (int k = 0; k < layoutOf(solo).bands; ++k) {
        n += aligned(oscBandFrames(k, frames) * (stereo ? 4 : 2)) + kOscBandTail;
    }
    return n;
}
#endif
