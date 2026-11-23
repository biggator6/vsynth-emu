// The band players' renders on the OSC voice (v332): BACKING's 0x893cadc0 and ENSEMBLE's 0x893ca1f0, rom.2's 0x8950e6e0
// and 0x8950db10 on rom.4's object, its words four bytes up from +0x94.  First rom.4's beat block 0x893c89a0
// (Vc2OscVoice::beatBlock, v326), whose tail leaves the walk step (+0x34); then XtMbPlayer's block on a copy of the
// object's words (their places below), the vpdt, the cached loop events and the bands read from the wave memory the
// voice's parts give, what it takes outside the voice's: its crossings by the voice's walk with the band players' row
// (index 16: the event jump 0x893cbdd0 and the cached one 0x893cbdf0, rom.2's 0x8950f6f0 and 0x8950f710, each on a
// copy again) and the walk list's wrap at an end; then the words back.  Every address is rom.4's, every offset the
// voice's but the words' places, rom.2's object's.  HARDWARE_FIRMWARE.md, "The VC-2's band players' renders".  Audio
// thread.
#include "Vc2OscPlayers.h"
#include "Vc2OscWalk.h"
#include "XtMbState.h"
#include <array>
#include <cfenv>
#include <cstddef>

namespace {
using State = XtMbPlayer::State;
using Band = XtMbPlayer::Band;
constexpr int kBacking = 3, kEnsemble = 5;   // [FW] the players' types, State::mode
constexpr int kBands = 6;
constexpr uint32_t kBandsAt = 0xe8, kBandBytes = 0x88;   // [FW] the bands' structs
// [FW] the object's byte 0: stereo; the walk wrapping at an end; the range in samples
constexpr uint8_t kStereo = 0x40, kWraps = 0x08, kInSamples = 0x80;
// [FW] the object's words that hold addresses (rom.2's places): the vpdt and the cached loop events; a band's data, and
// in BACKING's layout its segment list's next entry, the next segment's records and the next record, each the vpdt's
// address and the port's offset
constexpr uint32_t kVpdtAt = 0x10, kCachedAt = 0x6c, kDataAt = 0x00;
struct BandAddress {
    uint32_t at;
    long Band::*member;
};
constexpr std::array<BandAddress, 3> kAddresses { { { 0x1c, &Band::segList },
                                                    { 0x20, &Band::nextRecs },
                                                    { 0x24, &Band::recPtr } } };
// [FW] the cached loop events: four records of kXtMbLoopEventBytes, each a mode at +2 and a position at +4; the forward
// loop's end and the backward's start carry the loops' modes
constexpr int kCachedEvents = 4, kCachedMode = 2, kCachedPos = 4, kFwdEnd = 1, kBackStart = 3;

#define MB_WORD(at, m) XtWord { at, offsetof(State, m), sizeof(State::m) }
#define MB_BYTE(at, m) XtWord { at, offsetof(State, m), 1 }
#define MB_BAND(at, m) XtWord { at, offsetof(Band, m), sizeof(Band::m) }
// [FW] the player's words by their places in rom.2's object, but its wave's (the vpdt, the cached events, which hold
// addresses), the beat's (XtBeat's, Vc2OscBeat.cpp) and the loops' rates (+0xa0, +0xa4, which only rom.2's own
// crossings read, the walk's here): the flags, the end, the event kinds, the events' first half word, stride and slot
// (bytes, each in an int), a read's step; the current, last, first and end events; the start, end, last event's,
// current, next and crossing positions; the rate, the walk step, the position and its fraction; the pitch and walk;
// the gain's target, the gain and its rate; the smoother's coefficient, the rate and its loop factor; the walk list;
// the bands' outputs and the tree's state
constexpr std::array<XtWord, 33> kPlayerWords { {
    MB_WORD(0x00, flags0), MB_WORD(0x03, flags3), MB_WORD(0x04, ended), MB_WORD(0x05, flags5), MB_WORD(0x06, flags6),
    MB_BYTE(0x07, off7), MB_BYTE(0x08, stride8), MB_BYTE(0x09, stride9), MB_WORD(0x0a, step16), MB_WORD(0x0c, ev),
    MB_WORD(0x0e, evLast), MB_WORD(0x14, evStart), MB_WORD(0x16, evEnd), MB_WORD(0x18, startPos),
    MB_WORD(0x1c, endPos), MB_WORD(0x20, lastEvPos), MB_WORD(0x24, evPos), MB_WORD(0x28, nextEvPos),
    MB_WORD(0x2c, limit), MB_WORD(0x30, rate), MB_WORD(0x34, walkStep), MB_WORD(0x38, pos), MB_WORD(0x3c, posFrac),
    MB_WORD(0x70, pitch), MB_WORD(0x7c, walk), MB_WORD(0x80, gainTarget), MB_WORD(0x84, gain),
    MB_WORD(0x88, gainCoef), MB_WORD(0x94, lpc), MB_WORD(0x98, rateF), MB_WORD(0x9c, f9c), MB_WORD(0xa8, list),
    MB_WORD(0x418, M),
} };
// [FW] a band's from its +0, BACKING's (and every band 0's) but its addresses: the position's shift, the walk list's
// stride, the fraction's and the step's shifts (half words), the repeat budget, the grain's start, the next segment's,
// the record's start and end, the read's start offset, position, fraction and mask, the gain (its sign the way), the
// smoother's state
constexpr std::array<XtWord, 15> kBandWords { {
    MB_BAND(0x04, shift), MB_BAND(0x08, stride), XtWord { 0x0c, offsetof(Band, fshift), 2 },
    XtWord { 0x0e, offsetof(Band, sshift), 2 }, MB_BAND(0x10, budget), MB_BAND(0x14, grainStart),
    MB_BAND(0x18, nextSeg), MB_BAND(0x28, recStart), MB_BAND(0x2c, recEnd), MB_BAND(0x30, startOff),
    MB_BAND(0x34, readPos), MB_BAND(0x38, readFrac), MB_BAND(0x3c, mask), MB_BAND(0x40, gain), MB_BAND(0x44, lp),
} };
// [FW] ENSEMBLE's bands 1…5's: the position's shift, the stride, the frame, each channel's values and phases
constexpr std::array<XtWord, 7> kEnsembleWords { {
    MB_BAND(0x04, shift), MB_BAND(0x08, stride), MB_BAND(0x0c, frame),
    XtWord { 0x10, offsetof(Band, packed), sizeof(Band::packed[0]) },
    XtWord { 0x40, offsetof(Band, phase), sizeof(Band::phase[0]) },
    XtWord { 0x4c, offsetof(Band, packed) + sizeof(Band::packed[0]), sizeof(Band::packed[0]) },
    XtWord { 0x7c, offsetof(Band, phase) + sizeof(Band::phase[0]), sizeof(Band::phase[0]) },
} };
#undef MB_WORD
#undef MB_BYTE
#undef MB_BAND

uint32_t bandAt(int k) { return kBandsAt + kBandBytes * (uint32_t)k; }
// The object's word at its place in rom.2's object
uint32_t mbObjectWord(const Vc2OscVoice& v, uint32_t at) { return v.u32(vc2OscInRom4(at)); }

// Every word the block reads and writes, the player's then each band's at its place: BACKING's layout, or ENSEMBLE's
// for its bands 1…5
struct BandRows {
    std::array<XtWord, kPlayerWords.size() + kBands * kBandWords.size()> rows {};
    size_t count = 0;
    bool ensemble = false;
    std::span<const XtWord> words() const { return { rows.data(), count }; }
};
constexpr BandRows bandRowsOf(bool ensemble) {
    BandRows t;
    t.ensemble = ensemble;
    const auto add = [&t](std::span<const XtWord> words, size_t member, uint32_t at) {
        for (const XtWord& w : words) t.rows[t.count++] = { w.at + at, w.member + member, w.bytes };
    };
    add(kPlayerWords, 0, 0);
    for (int k = 0; k < kBands; ++k) {
        const std::span<const XtWord> words = ensemble && k > 0 ? std::span<const XtWord>(kEnsembleWords) : kBandWords;
        add(words, offsetof(State, b) + (size_t)k * sizeof(Band), kBandsAt + kBandBytes * (uint32_t)k);
    }
    return t;
}
constexpr BandRows kBackingRows = bandRowsOf(false), kEnsembleRows = bandRowsOf(true);
bool backingLayout(const BandRows& rows, int k) { return !rows.ensemble || k == 0; }

// The wave memory the voice's parts give, as bytes: the vpdt, the cached loop events and the bands lie in it
struct WaveBytes {
    const uint8_t* bytes;
    uint32_t from, size;
    uint32_t offsetOf(uint32_t address) const { return address - from; }   // past size: outside
    bool holds(uint32_t address, uint32_t n) const {
        return offsetOf(address) <= size && size - offsetOf(address) >= n;
    }
};
WaveBytes waveBytesOf(const Vc2OscSamples& s) {
    return { reinterpret_cast<const uint8_t*>(s.samples), s.address, s.count * (uint32_t)sizeof(int16_t) };
}
// n bytes from `address`, 0 where the memory holds none (rom.4 reads what RAM holds there)
void copyOut(const WaveBytes& m, uint32_t address, uint8_t* to, uint32_t n) {
    for (uint32_t i = 0; i < n; ++i) to[i] = m.holds(address + i, 1) ? m.bytes[m.offsetOf(address + i)] : 0;
}

// The vpdt from the object's +0x10 to the memory's end (a read before it reads 0, where rom.4 reads what memory holds:
// none reaches there, a wave's records lying past its events' table); each band's data from its +0x00, its base 0
void readBandWave(State& s, const Vc2OscVoice& v, const WaveBytes& m) {
    const uint32_t vpdt = mbObjectWord(v, kVpdtAt);
    s.w.channels = s.stereo ? 2 : 1;
    if (m.holds(vpdt, 0)) {
        s.w.vpdt = m.bytes + m.offsetOf(vpdt);
        s.w.vpdtBytes = m.size - m.offsetOf(vpdt);
    }
    const long halfwords = m.size / 2;
    for (int k = 0; k < kBands; ++k) {
        const uint32_t data = mbObjectWord(v, bandAt(k) + kDataAt);
        if (!m.holds(data, 0) || (m.offsetOf(data) & 1) != 0) continue;   // odd: an address error on the SH-4
        const long h = m.offsetOf(data) / 2;
        XtMbBand& d = s.w.band[k];
        d.data = reinterpret_cast<const int16_t*>(m.bytes) + h;
        d.lo = -(h / s.w.channels);
        d.hi = (halfwords - h) / s.w.channels;
    }
}

// The cached loop events from the object's +0x6c, their positions and the loops' modes
void readCached(State& s, const Vc2OscVoice& v, const WaveBytes& m) {
    const uint32_t at = mbObjectWord(v, kCachedAt);
    for (int r = 0; r < kCachedEvents; ++r) {
        copyOut(m, at + (uint32_t)(r * kXtMbLoopEventBytes), s.lrec[r], kXtMbLoopEventBytes);
        std::memcpy(&s.lp[r], s.lrec[r] + kCachedPos, sizeof s.lp[r]);
    }
    int16_t mode = 0;
    std::memcpy(&mode, s.lrec[kFwdEnd] + kCachedMode, sizeof mode);
    s.lmode1 = mode;
    std::memcpy(&mode, s.lrec[kBackStart] + kCachedMode, sizeof mode);
    s.lmode3 = mode;
}

void bandStateFrom(State& s, const Vc2OscVoice& v, const WaveBytes& m, const BandRows& rows) {
    vc2OscWordsFrom(v, &s, rows.words(), vc2OscInRom4);
    s.stereo = (s.flags0 & kStereo) != 0;
    s.stepGiven = true;
    readBandWave(s, v, m);
    readCached(s, v, m);
    const uint32_t vpdt = mbObjectWord(v, kVpdtAt);
    for (int k = 0; k < kBands; ++k) {
        for (const auto& [at, member] : kAddresses) s.b[k].*member = (int32_t)(mbObjectWord(v, bandAt(k) + at) - vpdt);
    }
}

void bandStateTo(const State& s, Vc2OscVoice& v, const BandRows& rows) {
    vc2OscWordsTo(v, &s, rows.words(), vc2OscInRom4);
    const uint32_t vpdt = mbObjectWord(v, kVpdtAt);
    for (int k = 0; k < kBands; ++k) {
        if (!backingLayout(rows, k)) continue;
        for (const auto& [at, member] : kAddresses) {
            v.setU32(vc2OscInRom4(bandAt(k) + at), vpdt + (uint32_t)(int32_t)(s.b[k].*member));
        }
    }
}

// An act on a copy of the object's words in BACKING's layout, ENSEMBLE's bands 1…5's words lying under its own as rom.4
// writes them, and the words back
template <class Act>
void onBandCopy(Vc2OscVoice& v, Vc2OscParts& p, Act act) {
    const WaveBytes m = waveBytesOf(p.waveSamples());
    State s;
    bandStateFrom(s, v, m, kBackingRows);
    act(s, m);
    bandStateTo(s, v, kBackingRows);
}
// [FW] 0x893cbdd0 (and the start's 0x893cbda0, its wrapper): the current event's records, rom.2's 0x8950f6f0
void bandJump(Vc2OscVoice& v, Vc2OscParts& p) {
    onBandCopy(v, p, [](State& s, const WaveBytes&) { s.initRecords(); });
}
// [FW] 0x893cbdf0: to a loop's cached event at `cached`, rom.2's 0x8950f710
void bandCached(Vc2OscVoice& v, Vc2OscParts& p, uint32_t cached) {
    onBandCopy(v, p, [cached](State& s, const WaveBytes& m) {
        uint8_t record[kXtMbLoopEventBytes];
        copyOut(m, cached, record, sizeof record);
        s.loopJump(record);
    });
}
// [FW] the start's seek for index 16 (the table at 0x8993fc00), 0x893cd440, rom.2's 0x89510bf0, still out by address
void bandSeek(Vc2OscVoice& v, Vc2OscParts& p, int32_t at) {
    constexpr uint32_t kBandSeek = 0x893cd440;
    v.seekBy(kBandSeek, at, p);
}
// [FW] index 16's row: the two jumps (0x893c9350, 0x893c9364), the step's sign kept at a turn, the start's seek and jump
// (0x8993fc00, 0x8993fc14)
const Vc2WalkWay kBandWalk { bandJump, bandCached, true, bandSeek, bandJump };

// What the block takes outside: the walk list's crossing on the voice's words (the state's words to the voice, the walk
// with the band players' row and the next crossing there, 0x893c8f50 or 0x893c9140 with index 16 then 0x893c9380, the
// words back), and its wrap at an end
class VoiceOutside final : public XtMbOutside {
public:
    VoiceOutside(Vc2OscVoice& v, Vc2OscParts& p, const WaveBytes& m, const BandRows& rows)
        : v_(v), p_(p), m_(m), rows_(rows) {}
    void cross(State& s, bool forward) override {
        bandStateTo(s, v_, rows_);
        if (forward) vc2WalkForward(v_, p_, kBandWalk);
        if (!forward) vc2WalkBackward(v_, p_, kBandWalk);
        vc2WalkCrossing(v_, p_);
        bandStateFrom(s, v_, m_, rows_);
    }
    // [FW] 0x893caf5e… and 0x893caff6…, rom.2's 0x8950e87e and 0x8950e916: at an end, with byte 0's 0x08 and not its
    // 0x80, the walk wraps: the position past the end from the far event's (the range's first going forward, its last
    // going back), that event's records (0x893cbdd0) and the next crossing (0x893c9380), the state's own routines
    bool wrapped(State& s, int r1, int r4, uint16_t to) override {
        if ((s.flags0 & kWraps) == 0 || (s.flags0 & kInSamples) != 0) return false;
        s.pos = (int)((uint32_t)r1 - (uint32_t)r4);
        s.ev = to;
        s.initRecords();
        s.crossings();
        return true;
    }

private:
    Vc2OscVoice& v_;
    Vc2OscParts& p_;
    const WaveBytes& m_;
    const BandRows& rows_;
};
} // namespace

// [FW] 0x893cadc0, 0x893ca1f0: the beat's block, then the player's block at round to zero (the SH-4's), mono or stereo
// by the object's byte 0
void vc2OscBands(Vc2OscVoice& v, float* out, Vc2OscParts& parts, bool ensemble) {
    v.beatBlock(parts);
    const WaveBytes m = waveBytesOf(parts.waveSamples());
    const BandRows& rows = ensemble ? kEnsembleRows : kBackingRows;
    State s;
    s.mode = ensemble ? kEnsemble : kBacking;
    bandStateFrom(s, v, m, rows);
    VoiceOutside outside(v, parts, m, rows);
    s.outside = &outside;
    const int rounding = std::fegetround();
    std::fesetround(FE_TOWARDZERO);
    XtMbPlayer::blockImpl(s, out);
    std::fesetround(rounding);
    bandStateTo(s, v, rows);
}
