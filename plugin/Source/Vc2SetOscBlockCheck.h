#pragma once
// xt_vc2sets' eighth part's shared words (v393, out of Vc2SetOscBlockCheck.cpp, v391, when it reached the kit's 300
// lines): where the part draws in Vc2Fw's RAM, rom.4's OSC table's layout and the bytes the draws aim at, and the
// draws (Vc2SetOscBlockDraws.cpp).  XT_VC2SETS_MAIN only.
#include "Vc2SetOscBlock.h"
#include "Vc2SetsChildCheck.h"
#include <array>
#include <span>

// In Vc2Fw's RAM: the glide keys and their key tables, the legato structs, the four getters' groups, the pitch source
inline constexpr uint32_t kOscGlidesAt = 0x8C037000, kOscGlideBytes = 0x40, kOscGlideTablesAt = 0x8C038000;
inline constexpr uint32_t kOscGlideTableBytes = 0x200, kOscLegatosAt = 0x8C039000, kOscLegatoBytes = 4;
inline constexpr uint32_t kOscGroupsAt = 0x8C03A000, kOscGroupBytes = 0x20, kOscSourceAt = 0x8C03B000;
inline constexpr uint32_t kOscObjects = 4, kOscParts = 16;
// [FW] the parts' OSC table: a part's row, its header, a kind's and a record's bytes, the systems after 16 rows; the
// step counts
inline constexpr uint32_t kOscTableAt = 0x89e371e0, kOscPartRow = 0x1020, kOscHeaderBytes = 32, kOscKindRow = 0x100;
inline constexpr uint32_t kOscRecordBytes = 0x80, kOscSystemsAt = kOscTableAt + kOscParts * kOscPartRow;
inline constexpr uint32_t kOscStepsAt = 0x89989c84;
// [FW] a key table's Hold, M's bend and D-Beam bytes, a voice's word for its glide key
inline constexpr uint32_t kOscHoldAt = 0x11c, kOscBendAt = 0x174, kOscBeamAt = 0x180, kOscSourceWordAt = 4;
// v395: the part table (rom.4's .data, 0xffffffff after C startup) and a trip object's word LITE's words read; the
// trip objects the draws aim it at, in Vc2Fw's RAM
inline constexpr uint32_t kOscPartTableAt = 0x8993d040, kOscTripMovingAt = 8;   // [FW]
inline constexpr uint32_t kOscTripsAt = 0x8C03C000, kOscTripBytes = 0x28;
// v396: the wave's events' tables VariPhrase's words read, in Vc2Fw's RAM: noise at a case's start, an input's table
// pointer within its first 256 bytes, so its entries (from +12, up to 65,536 of two bytes) stay in it
inline constexpr uint32_t kOscEventsAt = 0x8C040000, kOscEventsBytes = 0x22000, kOscEventsAims = 0x100;
// [FW] v397: the words by a set's index PCM's start calls (hooked until v399, then rom.4's own as the store's three are)
inline constexpr uint32_t kOscBySetAt = 0x8939cba0;
// v398: the wave store's records and their chunks' blocks, in Vc2Fw's RAM
inline constexpr uint32_t kOscWaveRecordsAt = 0x8C064000, kOscWaveRecordBytes = 0x80, kOscWaveRecords = 128;
inline constexpr uint32_t kOscBlocksAt = 0x8C068000, kOscBlockBytes = 0x80, kOscBlocks = 192;
// v398: a page at 0, mapped and noise: a wave the store has no vpno for leaves its events' table 0, which VariPhrase's
// words then read under STEP or EVENT from +12 (area 0 on the XT, unknown here; the same on both sides); v399: by a
// set's index its events' count too is read from there (+4), so the page holds a halfword count's entries
inline constexpr uint32_t kOscZeroPageBytes = 0x21000;

// The parts' OSC table (every header and record at a case's first draw, now and then a part's; the systems), the glide
// keys, legato structs, Ms' bytes, groups and pitch source, an input, one of the four Ms, and the refresh's records
void oscTableDrawn(Vc2Fw& fw, std::mt19937& rng, bool all);
void oscAroundDrawn(Vc2Fw& fw, std::mt19937& rng, bool all);
void oscInputDrawn(Vc2SetOscInput& w, std::mt19937& rng);
uint32_t oscMatrixDrawn(std::mt19937& rng);
void oscSameRecordsDrawn(Vc2Fw& fw, std::span<const Vc2SetOscInput> inputs, std::mt19937& rng);
// v398 (Vc2SetOscStoreDraws.cpp): the wave store, its words into the port's `store` and its records and blocks into
// rom.4's memory (all, or now and then); a Wave Number about the store's (0…40, now and then 128 or 256 up); v399: the
// sets, their counts, tables and buffers into the port's `sets` and their streams' data into rom.4's memory, and a
// set's number about their counts (1…48 mostly, now and then 0 or any halfword)
void oscStoreDrawn(Vc2Fw& fw, Vc2WaveStore& store, std::mt19937& rng, bool all);
uint16_t oscStoreNumber(std::mt19937& rng);
void oscSetsDrawn(Vc2Fw& fw, Vc2WaveSets& sets, std::mt19937& rng, bool all);
uint16_t oscSetNumber(std::mt19937& rng);
