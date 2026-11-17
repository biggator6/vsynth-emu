// rom.4's Matrix object, as far as the COSM converters reach it, and the Matrix core: see Vc2Matrix.h (v235, v243).
// Every address is rom.4's.
#include "Vc2Matrix.h"
#include <algorithm>

namespace {
constexpr int kTermShift = 13;   // [FW] a term's depth × value >> 13
// [FW] the groups' rows, by their readers in Vc2Matrix.h's order
constexpr Vc2MatrixGroup kGroups[] { { 0, 1, 0xf }, { 1, 5, 0xf }, { 2, 9, 0x7 }, { 3, 12, 0xfff }, { 4, 24, 0x1f },
                                     { 5, 29, 0xfff }, { 6, 41, 0x1f }, kVc2MatrixCosm1, kVc2MatrixCosm2,
                                     { 10, 68, 0x7ff } };

// A slot's term for a source's value: depth × value >> 13 (arithmetic), held to ± its destination's limit.  rom.4 reads
// the limits at any destination byte; past the table's 79 it reads the next table, which no set-up reaches (the port 0)
int16_t slotTerm(const Vc2MatrixSlot& s, int16_t value) {
    const int32_t t = (s.depth * value) >> kTermShift;
    const int32_t most = s.dest < Vc2MatrixTables::kDestinations ? vc2MatrixTables().limits[s.dest] : 0;
    return static_cast<int16_t>(t > most ? most : t >= -most ? t : -most);
}
} // namespace

std::span<const Vc2MatrixGroup> vc2MatrixGroups() { return kGroups; }

void Vc2MatrixCore::refresh(Vc2MatrixWords& m) {
    bool moved = false;
    for (int k = 0; k < kSources; ++k) {
        const auto value = static_cast<int16_t>(sources_.value(m.routines[k], m.sources[k]));
        if (value == m.last[k]) continue;
        m.last[k] = value;
        moved = true;
        for (int j = 2 * k; j < 2 * k + 2; ++j) m.terms[j] = value == 0 ? 0 : slotTerm(m.slots[j], value);
    }
    if (moved) std::ranges::copy(m.uses, m.dirty);
}

int32_t vc2MatrixSum(const Vc2MatrixWords& m, int dest) {
    int32_t sum = 0;
    for (int k = 0; k < Vc2MatrixWords::kSlots; ++k) {
        if (m.slots[k].dest == dest) sum += m.terms[k];
    }
    const int32_t most = vc2MatrixTables().limits[dest];
    return sum > most ? most : sum < -most ? -most : sum;
}

const int16_t* vc2MatrixRead(Vc2MatrixWords& m, const Vc2MatrixGroup& g, Vc2MatrixRefresh& refresh) {
    int16_t* sums = &m.sums[g.first - 1];
    if ((m.uses[g.index] & g.bits) == 0) return sums;
    refresh.refresh(m);
    const auto bits = static_cast<uint16_t>(m.dirty[g.index] & g.bits);
    if (bits == 0) return sums;
    m.dirty[g.index] = static_cast<uint16_t>(m.dirty[g.index] & ~bits);
    for (int k = 0; k < 16; ++k) {
        if (bits & (1u << k)) sums[k] = static_cast<int16_t>(vc2MatrixSum(m, g.first + k));
    }
    return sums;
}

const int16_t* Vc2CosmMatrixReader::sums(bool second, uint32_t) {
    return vc2MatrixRead(m_, second ? kVc2MatrixCosm2 : kVc2MatrixCosm1, refresh_);
}
