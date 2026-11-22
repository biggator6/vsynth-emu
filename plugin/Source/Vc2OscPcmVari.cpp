// VariPhrase's words: see Vc2OscPcmVari.h (v396).  The cents in XtFloat, as the SH-4 rounds; the integer sums in
// uint32_t, as its registers hold them; the halfword stores and their casts as rom.4 makes them.  Every address is
// rom.4's.
#include "Vc2OscPcmVari.h"
#include "Vc2InputReads.h"
#include "Vc2OscPcmLite.h"
#include "XtFloat.h"
#include <bit>
#include <cstring>

namespace {
using vc2in::s16;
using vc2in::u32;
using vc2in::u8;

// one-off: rom.4's offsets in the converter's input, declarations, not a job to template
// [FW] the input's words: the key and the pitch10s; the record's Start Offset, Tempo Sync, Robot Voice, Playback Mode,
// Loop, Vari, Energy, Time Trip and Beat Keep; the wave's table flag, type, a word, its no-wave flag, events' count,
// length, events' table and 48 bytes; the step count
constexpr int kKeyAt = 3, kPitch10At = 0x94, kKeyPitch10At = 0x98, kStartAt = 0x48, kTempoSyncAt = 0x60;
constexpr int kRobotAt = 0x61, kModeAt = 0x62, kLoopAt = 0x63, kVariAt = 0x64, kEnergyAt = 0x66, kTimeTripAt = 0x67;
constexpr int kBeatKeepAt = 0x68, kTableFlagAt = 0xa0, kTypeAt = 0xa7, kWordAcAt = 0xac, kNoWaveAt = 0xb0;
constexpr int kEventsAt = 0xb4, kLengthAt = 0xbc, kTableAt = 0xc4, kWaveBytesAt = 0xcc, kStepAt = 0x104;
constexpr uint32_t kEventCentreAt = 4;        // [FW] the systems' +0x10204 (the part's reader clears it)
constexpr uint32_t kStep = 2;                 // [FW] the Playback Mode the switch starts from: STEP, then EVENT
constexpr uint8_t kTypeOne = 1;               // [FW] the wave type with its own cents
constexpr uint32_t kEntriesAt = 12;           // [FW] the events' table's first entry
constexpr int kCentre = 60;                   // [FW] the keys' centre
constexpr int kPeriodKeys = 12, kRound = 11;  // [FW] EVENT's period: the count rounded up to twelve keys
constexpr uint32_t kA4 = 69000;               // [FW] 0x8939f8d4 (−69000 added): pitch10 at A4
// [FW] the block's bytes and words
constexpr size_t kFlagsByte = 0xe4, kFlags5Byte = 0xe5, kFlags6Byte = 0xe6, kFirstAt = 0xe8, kLastAt = 0xec;
constexpr size_t kFromAt = 0xf0, kCentsWord = 0x08 / 4, kKeyCentsWord = 0x68 / 4, kLevelWord = 0x18 / 4;
constexpr size_t kTypeWord = 0xb0 / 4, kWaveBytesWord = 0xb4 / 4, kWaveBytes = 48;
// [FW] +0xe4's bits: Loop (0), Tempo Sync (1), Time Trip (2), cleared (3), a fixed pitch (4), Vari off (5), the word
// +0xac (6), the default's (7); +0xe5's: Beat Keep (7); +0xe6's three cleared
constexpr uint8_t kBit0 = 0x01, kBit1 = 0x02, kBit2 = 0x04, kBit3 = 0x08, kBit4 = 0x10, kBit5 = 0x20, kBit6 = 0x40;
constexpr uint8_t kBit7 = 0x80;

uint8_t* blockBytes(uint32_t* block) { return reinterpret_cast<uint8_t*>(block); }

uint16_t inputHalf(const uint8_t* input, int at) { return static_cast<uint16_t>(s16(input, at)); }

void storeHalf(uint32_t* block, size_t at, uint16_t v) { std::memcpy(blockBytes(block) + at, &v, sizeof v); }

void storeWord(uint32_t* block, size_t at, uint32_t v) { std::memcpy(blockBytes(block) + at, &v, sizeof v); }

void bitOf(uint8_t& b, uint8_t bit, bool on) {
    b = on ? static_cast<uint8_t>(b | bit) : static_cast<uint8_t>(b & ~bit);
}

// Two events' entries of the wave's table into +0xe8 and +0xec, and the first again into +0xf0
void eventsFrom(const uint8_t* input, uint32_t first, uint32_t last, Vc2SetReads& reads, uint32_t* block) {
    const uint32_t table = u32(input, kTableAt) + kEntriesAt;
    const uint16_t from = reads.waveHalf(table + 2 * first);
    storeHalf(block, kFirstAt, from);
    storeHalf(block, kLastAt, reads.waveHalf(table + 2 * last));
    storeHalf(block, kFromAt, from);
}

// 0x8939c850: EVENT's entry for the key, its offset from the centre in periods of `period` keys, within the count
uint16_t eventOf(int key, uint16_t count, uint16_t period) {
    if (count == 0) return 0;
    int within = (key - kCentre) % period;
    if (key < kCentre) within = (within + period) % period;
    return static_cast<uint16_t>(static_cast<uint16_t>(within) % count);
}

// EVENT's centre with the systems' byte set: 60 moved by whole periods to the key
int eventCentre(int key, uint16_t period) {
    if (period == 0) return kCentre;
    const int from = key >= kCentre ? key - kCentre : key - (kCentre - 1) - period;
    const auto periods = static_cast<int16_t>(from / period);
    return static_cast<int16_t>(periods * static_cast<int16_t>(period) + kCentre);
}

// STEP's and EVENT's start: +0xe4's bits 0, 7 and 3 cleared
void modeFlags(uint32_t* block) {
    uint8_t& f = blockBytes(block)[kFlagsByte];
    f = static_cast<uint8_t>(f & ~(kBit0 | kBit7 | kBit3));
}

// RETRIGGER and LEGATO (0x8939f6f0): Loop or no wave into bit 0, bit 7 set and 3 cleared, the Start Offset held below
// the length, the length, the pitch about 60
void defaultWay(uint8_t* input, Vc2SetReads&, uint32_t* block) {
    uint8_t& f = blockBytes(block)[kFlagsByte];
    bitOf(f, kBit0, u8(input, kLoopAt) != 0 || u8(input, kNoWaveAt) == 0);
    f = static_cast<uint8_t>((f | kBit7) & ~kBit3);
    const uint32_t start = u32(input, kStartAt), length = u32(input, kLengthAt);
    const uint32_t held = start >= length ? length - 1 : start;
    storeWord(block, kFirstAt, held);
    storeWord(block, kLastAt, length);
    storeWord(block, kFromAt, held);
    vc2OscPitchAbout(input, kCentre, block);
}

// STEP (0x8939f5d0): under two events the default's way; else the events from the step count's within the count less
// one
void stepWay(uint8_t* input, Vc2SetReads& reads, uint32_t* block) {
    if (inputHalf(input, kEventsAt) < 2) {
        defaultWay(input, reads, block);
        return;
    }
    modeFlags(block);
    const uint32_t count = inputHalf(input, kEventsAt) - 1u;
    const auto e = static_cast<uint16_t>(static_cast<int32_t>(inputHalf(input, kStepAt)) % static_cast<int32_t>(count));
    eventsFrom(input, e, e + 1u, reads, block);
    vc2OscPitchAbout(input, kCentre, block);
}

// EVENT (0x8939f2f0): with no table flag the first and last events; with it the key's by the period, the pitch about
// a centre the systems' byte moves to the key
void eventWay(uint8_t* input, Vc2SetReads& reads, uint32_t* block) {
    modeFlags(block);
    const uint16_t events = inputHalf(input, kEventsAt);
    if (u32(input, kTableFlagAt) == 0) {
        eventsFrom(input, 0, events - 1u, reads, block);
        vc2OscPitchAbout(input, kCentre, block);
        return;
    }
    const auto count = static_cast<uint16_t>(events - 1);
    const auto period = static_cast<uint16_t>((count + kRound) / kPeriodKeys * kPeriodKeys);
    const int key = u8(input, kKeyAt);
    const uint16_t e = eventOf(key, count, period);
    eventsFrom(input, e, e + 1u, reads, block);
    vc2OscPitchAbout(input, reads.oscSystems()[kEventCentreAt] == 0 ? kCentre : eventCentre(key, period), block);
}

// The cents of both pitch10s: a type 1 with Vari and Robot Voice on against A4 (+0xe4's bit 4), else against the
// wave's root; the type 1's +0xb0 1, the others' their type
void centsWords(const uint8_t* input, uint32_t* block) {
    const Vc2OscConvertTables& t = vc2OscConvertTables();
    const bool one = u8(input, kTypeAt) == kTypeOne;
    const bool fixed = one && u8(input, kVariAt) != 0 && u8(input, kRobotAt) != 0;
    block[kTypeWord] = one ? 1u : u8(input, kTypeAt);
    bitOf(blockBytes(block)[kFlagsByte], kBit4, fixed);
    const Vc2OscCentsHold& hold = fixed ? t.variA4 : one ? t.variRoot : t.variRootOther;
    const uint32_t from = fixed ? kA4 : vc2OscRootPitch10(input);
    block[kCentsWord] = std::bit_cast<uint32_t>(vc2OscCentsFrom(u32(input, kPitch10At), from, hold));
    block[kKeyCentsWord] = std::bit_cast<uint32_t>(vc2OscCentsFrom(u32(input, kKeyPitch10At), from, hold));
}

// The last words: +0xe4's bits 5, 6, 2 and 1, +0xe5's bit 7, +0xe6's three cleared, the level and the wave's bytes
void lastWords(const uint8_t* input, uint32_t tripMoving, uint32_t* block) {
    const Vc2OscConvertTables& t = vc2OscConvertTables();
    uint8_t& f = blockBytes(block)[kFlagsByte];
    bitOf(f, kBit5, u8(input, kVariAt) == 0);
    const float level = xtf::div(xtf::flt(vc2OscLevelWord(t, u8(input, kEnergyAt))), t.variLevel);
    block[kLevelWord] = std::bit_cast<uint32_t>(level);
    std::memcpy(block + kWaveBytesWord, input + kWaveBytesAt, kWaveBytes);
    bitOf(f, kBit6, u32(input, kWordAcAt) != 0);
    bitOf(f, kBit2, tripMoving != 0 && u8(input, kTimeTripAt) != 0);
    bitOf(f, kBit1, u8(input, kTempoSyncAt) != 0);
    bitOf(blockBytes(block)[kFlags5Byte], kBit7, u8(input, kBeatKeepAt) != 0);
    uint8_t& g = blockBytes(block)[kFlags6Byte];
    g = static_cast<uint8_t>(g & ~(kBit2 | kBit1 | kBit0));
}

using ModeWay = void (*)(uint8_t*, Vc2SetReads&, uint32_t*);
// [FW] 0x8939f2c2's switch on the Playback Mode less 2, unsigned: STEP, EVENT; past them the default
constexpr std::array<ModeWay, 2> kModeWays { stepWay, eventWay };
} // namespace

void vc2OscPcmVari(uint8_t* input, Vc2SetReads& reads, uint32_t* block) {
    const uint32_t way = u8(input, kModeAt) - kStep;
    (way < kModeWays.size() ? kModeWays[way] : defaultWay)(input, reads, block);
    centsWords(input, block);
    lastWords(input, reads.tripMoving(input[0]), block);
}
