#pragma once
// xt_vc2sets' eighth part's shared words (v393, out of Vc2SetOscBlockCheck.cpp, v391, when it reached the kit's 300
// lines): where the part draws in Vc2Fw's RAM, rom.4's OSC table's layout and the bytes the draws aim at, and the
// draws (Vc2SetOscBlockDraws.cpp).  XT_VC2SETS_MAIN only.
#include "Vc2SetOscBlock.h"
#include "Vc2SetsChildCheck.h"
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

// The parts' OSC table (every header and record at a case's first draw, now and then a part's; the systems), the glide
// keys, legato structs, Ms' bytes, groups and pitch source, an input, one of the four Ms, and the refresh's records
void oscTableDrawn(Vc2Fw& fw, std::mt19937& rng, bool all);
void oscAroundDrawn(Vc2Fw& fw, std::mt19937& rng, bool all);
void oscInputDrawn(Vc2SetOscInput& w, std::mt19937& rng);
uint32_t oscMatrixDrawn(std::mt19937& rng);
void oscSameRecordsDrawn(Vc2Fw& fw, std::span<const Vc2SetOscInput> inputs, std::mt19937& rng);
