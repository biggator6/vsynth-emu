// rom.4's Matrix object, as far as the COSM converters reach it: see Vc2Matrix.h (v235).  Every address is rom.4's.
#include "Vc2Matrix.h"

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
