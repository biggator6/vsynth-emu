// C's reads of memory at rom.4's addresses: see Vc2SetMemoryReads.h (v341, moved from xt_vc2sets' SetPortParts).
#include "Vc2SetMemoryReads.h"
#include <cstring>

Vc2SetVoice Vc2SetMemoryReads::voiceAt(uint32_t channelAt) { return value<Vc2SetVoice>(channelAt); }
uint8_t Vc2SetMemoryReads::firstByte(uint32_t object) { return rd8(object); }
uint32_t Vc2SetMemoryReads::channelOf(uint32_t keys) { return rd32(keys); }
int16_t Vc2SetMemoryReads::bendOf(uint32_t channel) { return static_cast<int16_t>(rd16(channel)); }

uint8_t Vc2SetMemoryReads::controlOf(uint32_t channel, uint32_t number) {
    return rd8(channel + kVc2SetControls + number);
}

uint8_t Vc2SetMemoryReads::modeOf(uint32_t part, uint32_t kind) { return rd8(kVc2SetModesAt + 16 * part + kind); }

uint8_t Vc2SetMemoryReads::matrixControl(uint32_t matrix, uint32_t n) {
    return rd8(matrix + kVc2SetMatrixControls + n);
}

std::array<uint8_t, 60> Vc2SetMemoryReads::oscSystems() { return value<std::array<uint8_t, 60>>(kVc2OscSystemsAt); }
uint8_t Vc2SetMemoryReads::holdOf(uint32_t keys) { return rd8(keys + kVc2OscHoldAt); }
std::array<uint8_t, 2> Vc2SetMemoryReads::legatoOf(uint32_t word) { return { rd8(word + 1), rd8(word + 2) }; }

int32_t Vc2SetMemoryReads::matrixBend(uint32_t matrix) {
    return static_cast<int32_t>(rd32(matrix + kVc2OscBendAt));
}

std::array<uint16_t, 3> Vc2SetMemoryReads::commonOf(uint32_t part) {
    return value<std::array<uint16_t, 3>>(kVc2SetCommonsAt + kVc2SetCommonBytes * part);
}

std::array<uint16_t, 5> Vc2SetMemoryReads::envelopeOf(uint32_t part, uint32_t kind, uint32_t index) {
    return partWords<5>(kVc2SetEnvelopeTable, part, kind, index);
}

std::array<uint16_t, 5> Vc2SetMemoryReads::lfoOf(uint32_t part, uint32_t kind, uint32_t index) {
    return partWords<5>(kVc2SetLfoTable, part, kind, index);
}

std::array<uint16_t, 11> Vc2SetMemoryReads::mixOf(uint32_t part, uint32_t kind) {
    return partWords<11>(kVc2SetMixTable, part, kind, 0);
}

std::array<uint16_t, 12> Vc2SetMemoryReads::tvaOf(uint32_t part, uint32_t kind) {
    return partWords<12>(kVc2SetTvaTable, part, kind, 0);
}

std::array<uint16_t, 21> Vc2SetMemoryReads::cosmInputOf(uint32_t part, uint32_t kind, uint32_t index) {
    return partWords<21>(kVc2SetCosmTable, part, kind, index);
}

// [FW] the trailer's gain, rate and route at +0, +2 and +4
Vc2SetTvaGlobals Vc2SetMemoryReads::tvaGlobals() {
    Vc2SetTvaGlobals g;
    g.controls = rd8(kVc2SetTvaFlagAt);
    g.gain = static_cast<int8_t>(rd8(kVc2SetTrailerAt));
    g.rate = static_cast<int16_t>(rd16(kVc2SetTrailerAt + 2));
    g.route = rd8(kVc2SetTrailerAt + 4);
    return g;
}

// [FW] the clock object's source (+0x40), the internal tempo's numerator and denominator (+0x10, +0x14), the period
// (+0x48)
Vc2SetClock Vc2SetMemoryReads::clock() {
    Vc2SetClock c;
    c.external = rd8(kVc2SetClockAt + 0x40);
    c.numerator = (int32_t)rd32(kVc2SetClockAt + 0x10);
    c.denominator = (int32_t)rd32(kVc2SetClockAt + 0x14);
    c.period = rd16(kVc2SetClockAt + 0x48);
    return c;
}

std::array<uint8_t, 32> Vc2SetMemoryReads::oscHeader(uint32_t part) {
    return value<std::array<uint8_t, 32>>(kVc2OscTableAt + kVc2OscPartRow * part);
}

std::array<uint8_t, 128> Vc2SetMemoryReads::oscRecord(uint32_t part, uint32_t kind, uint32_t osc) {
    const uint32_t slot = kVc2OscHeaderBytes + kVc2OscKindRow * kind + kVc2OscRecordBytes * osc;
    return value<std::array<uint8_t, 128>>(kVc2OscTableAt + kVc2OscPartRow * part + slot);
}

// [FW] a glide key's key table (+0), its last POLY key and held flag (+0x1a, +0x1b), its last MONO key and the same
// (+0x20, +0x21)
Vc2SetGlide Vc2SetMemoryReads::glideAt(uint32_t glide) {
    const auto b = value<std::array<uint8_t, 0x24>>(glide);
    Vc2SetGlide g;
    std::memcpy(&g.table, b.data(), sizeof g.table);
    g.prevPoly = b[0x1a];
    g.prevPolyHeld = b[0x1b];
    g.prevMono = b[0x20];
    g.prevMonoHeld = b[0x21];
    return g;
}

uint32_t Vc2SetMemoryReads::tripMoving(uint8_t part) {
    return rd32(rd32(kVc2OscPartTableAt + 4u * part) + kVc2OscTripMovingAt);
}
