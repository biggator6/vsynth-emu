#pragma once
// What the part's settings' readers share (v288; Vc2PartSettings.cpp's until then): the held flags each tests, a
// table's byte and halfword stores, a product as mul.l keeps it, and each kind's rows in turn.  The notes' task.
#include <cstdint>
#include <functional>

namespace Vc2PartWords {
constexpr int kKinds = 16;                                                       // [FW] a part's note kinds
constexpr int kHeldCommon = 0, kHeldKind = 1, kHeldEnvelope = 2, kHeldLfo = 3;   // [FW] the flags each tests

void put8(uint8_t* at, int32_t v);
void put16(uint8_t* at, int32_t v);
// A product in 32 bits, as mul.l keeps it
int32_t times(int32_t v, int32_t by);
// Each kind's `rows` rows in turn, the first that gives up stopping the rest
bool everyRow(int rows, const std::function<bool(int, int)>& row);
} // namespace Vc2PartWords
