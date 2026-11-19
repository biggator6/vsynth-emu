// The VC-2 part's settings by ID: see Vc2PartSettings.h (v286).  Each reader's flag tests, reads and stores in rom.4's
// order.
#include "Vc2PartSettings.h"
#include "Vc2PartSettingsTables.h"
#include "Vc2SetChildHead.h"
#include <cstring>

namespace {
using Block = Vc2ParamBlock;
constexpr int kKinds = 16, kEnvelopes = 13, kLfos = 5;            // [FW] a part's note kinds, a kind's rows
constexpr int kHeldCommon = 0, kHeldKind = 1, kHeldEnvelope = 2, kHeldLfo = 3;   // [FW] the flags each tests
constexpr int kTvaEnvelope = 12;                                  // [FW] the envelope whose on flag the part sets
constexpr uint32_t kCommonBytes = 6;                              // [FW] the tables' strides
constexpr uint32_t kEnvelopePart = 2080, kEnvelopeKind = 130, kEnvelopeBytes = 10;
constexpr uint32_t kLfoPart = 800, kLfoKind = 50, kLfoBytes = 10, kLfoSystemAt = 0x3200;
constexpr uint32_t kMixPart = 352, kMixKind = 22;
constexpr uint32_t kTvaPart = 384, kTvaKind = 24, kTvaGainAt = 0x1800, kTvaRateAt = 0x1802, kTvaRouteAt = 0x1804;
// [FW] the IDs: the common's; the envelopes' on flag and first, 13 an envelope; the LFOs' first, 8 an LFO; the mix's
// first; the TVA's first by kind and its part's, its velocity's two; the systems'
constexpr int16_t kCommonIds[] = { 0x132, 0x134, 0x135 }, kCommonSens = 0x136;
constexpr int16_t kEnvelopeOn = 0x2d9, kEnvelopeIds = 0x390, kEnvelopeStride = 13;
constexpr int16_t kLfoIds = 0x439, kLfoStride = 8, kMixIds = 0x2d4, kTvaIds = 0x2d9, kTvaGainId = 0x195;
constexpr int16_t kCurveId = 0x432, kDepthId = 0x433;
constexpr int16_t kLfoSystemId = 67, kTvaRouteId = 66, kTvaTrailerGain = 0x533, kTvaTrailerRate = 0x52c;
constexpr int16_t kControlsId = 0xbb;
constexpr int32_t kSixtyThird = static_cast<int32_t>(0x82082083u);   // [FW] n / 63 by its shift
constexpr int kSixtyThirdShift = 5;
constexpr int32_t kThirtyFirst = static_cast<int32_t>(0x84210843u);  // [FW] n / 31 by its shift
constexpr int kThirtyFirstShift = 4, kThirtySecondShift = 5;
constexpr int32_t kHundred = 100, kTen = 10, kDepthEdge = 32;
// [FW] v287's: the COSM inputs' flag, count, strides and IDs (a type's run after the type), the bytes' ID; each COSM
// input's two envelopes whose velocity curve and depth it takes (envelope i's at 0x389 + 13 i and the next)
constexpr int kHeldCosm = 4, kCosms = 2;
constexpr uint32_t kBytesPart = 16, kCosmPart = 0x540, kCosmKind = 84, kCosmBytes = 42, kCosmIdsAt = 10;
constexpr int16_t kBytesId = 0x2d3, kCosmFirst = 0x462, kCosmTypeId = 0x463, kVelocityIds = 0x389;
constexpr int kCosmEnvelopes[kCosms][2] = { { 9, 10 }, { 11, 12 } };

void put8(uint8_t* at, int32_t v) { *at = static_cast<uint8_t>(v); }

void put16(uint8_t* at, int32_t v) {
    const uint16_t h = static_cast<uint16_t>(v);
    std::memcpy(at, &h, sizeof h);
}

// A product in 32 bits, as mul.l keeps it
int32_t times(int32_t v, int32_t by) {
    return static_cast<int32_t>(static_cast<uint32_t>(v) * static_cast<uint32_t>(by));
}

// A value × 100 / 63, as rom.4 divides
int32_t percentOf63(int32_t v) { return vc2ByMagic(times(v, kHundred), kSixtyThird, kSixtyThirdShift); }

// Each kind's rows in turn, the first that gives up stopping the rest
template <class Row>
bool everyRow(int rows, Row row) {
    for (int kind = 0; kind < kKinds; ++kind)
        for (int i = 0; i < rows; ++i)
            if (!row(kind, i)) return false;
    return true;
}
} // namespace

uint8_t vc2CurveOf(int32_t x) {
    const uint32_t code = static_cast<uint32_t>(x) - 1u;
    return static_cast<uint8_t>(code > 6 ? 7 : code);
}

int32_t vc2DepthOf(int32_t x) {
    if (x < -kDepthEdge)
        return -(kHundred + vc2ByMagic(times(static_cast<uint8_t>(-kDepthEdge - x), kHundred), kThirtyFirst,
                                       kThirtyFirstShift));
    if (x < 0) return -(times(static_cast<uint8_t>(-x), kHundred) >> kThirtySecondShift);
    if (x <= kDepthEdge) return times(static_cast<uint8_t>(x), kHundred) >> kThirtySecondShift;
    return kHundred + vc2ByMagic(times(static_cast<uint8_t>(x - kDepthEdge), kHundred), kThirtyFirst,
                                 kThirtyFirstShift);
}

bool Vc2PartSettings::common(Vc2PartRows& t, int part) {
    if (p_.held(kHeldCommon)) return false;
    uint8_t* row = t.commons.data() + kCommonBytes * part;
    for (int k = 0; k < 3; ++k) put8(row + k, p_.value(Block::kPart, part, 0, kCommonIds[k]));
    put16(row + 4, p_.value(Block::kPart, part, 0, kCommonSens));
    return !p_.held(kHeldCommon);
}

bool Vc2PartSettings::envelopeRow(Vc2PartRows& t, int part, int kind, int i) {
    uint8_t* row = t.envelopes.data() + kEnvelopePart * part + kEnvelopeKind * kind + kEnvelopeBytes * i;
    if (i != kTvaEnvelope) {
        row[0] = 1;
    } else {
        if (p_.held(kHeldKind)) return false;
        put8(row, p_.value(Block::kKindA, part, kind, kEnvelopeOn));
        if (p_.held(kHeldKind)) return false;
    }
    if (p_.held(kHeldEnvelope)) return false;
    const auto get = [&](int at) { return p_.value(Block::kKindB, part, kind, (int16_t)(kEnvelopeIds + at)); };
    const int b = kEnvelopeStride * i;
    put8(row + 1, get(b));
    put8(row + 2, get(b + 2));
    put8(row + 3, get(b + 3));
    put8(row + 4, get(b + 5));
    put8(row + 5, percentOf63(get(b + 8)));
    put8(row + 6, percentOf63(get(b + 9)));
    put8(row + 7, percentOf63(get(b + 0xa)));
    put16(row + 8, times(get(b + 0xb), kTen));
    return !p_.held(kHeldEnvelope);
}

bool Vc2PartSettings::envelopes(Vc2PartRows& t, int part) {
    return everyRow(kEnvelopes, [&](int kind, int i) { return envelopeRow(t, part, kind, i); });
}

bool Vc2PartSettings::lfoRow(Vc2PartRows& t, int part, int kind, int i) {
    if (p_.held(kHeldLfo)) return false;
    uint8_t* row = t.lfos.data() + kLfoPart * part + kLfoKind * kind + kLfoBytes * i;
    const auto get = [&](int at) { return p_.value(Block::kKindC, part, kind, (int16_t)(kLfoIds + at)); };
    const int b = kLfoStride * i;
    put8(row, get(b));
    put16(row + 2, get(b + 2));
    for (int k = 3; k <= 7; ++k) put8(row + 1 + k, get(b + k));
    return !p_.held(kHeldLfo);
}

bool Vc2PartSettings::lfos(Vc2PartRows& t, int part) {
    return everyRow(kLfos, [&](int kind, int i) { return lfoRow(t, part, kind, i); });
}

void Vc2PartSettings::lfoSystem(Vc2PartRows& t) {
    put8(&t.lfos[kLfoSystemAt], p_.value(Block::kSystemB, 0, 0, kLfoSystemId));
}

bool Vc2PartSettings::mixRow(Vc2PartRows& t, int part, int kind) {
    if (p_.held(kHeldKind)) return false;
    uint8_t* row = t.mixes.data() + kMixPart * part + kMixKind * kind;
    for (int k = 0; k < 5; ++k) put8(row + k, p_.value(Block::kKindA, part, kind, (int16_t)(kMixIds + k)));
    return !p_.held(kHeldKind);
}

bool Vc2PartSettings::mixes(Vc2PartRows& t, int part) {
    return everyRow(1, [&](int kind, int) { return mixRow(t, part, kind); });
}

bool Vc2PartSettings::tvaRow(Vc2PartRows& t, int part, int kind) {
    if (p_.held(kHeldKind)) return false;
    uint8_t* row = t.tvas.data() + kTvaPart * part + kTvaKind * kind;
    const auto get = [&](int at) { return p_.value(Block::kKindA, part, kind, (int16_t)(kTvaIds + at)); };
    put8(row + 4, get(0));
    put8(row + 6, get(1));
    put16(row + 8, times(get(2), kTen));
    put8(row + 0xa, get(3));
    put8(row + 0xc, get(4));
    put16(row + 0xe, times(get(5), kTen));
    put8(row + 0x10, get(6));
    for (int k = 0; k < 4; ++k) put8(row + 0x12 + k, get(7 + k));
    if (p_.held(kHeldKind) || p_.held(kHeldCommon)) return false;
    put8(row + 0x16, p_.value(Block::kPart, part, 0, kTvaGainId));
    return !p_.held(kHeldCommon);
}

bool Vc2PartSettings::tvaVelocityRow(Vc2PartRows& t, int part, int kind) {
    if (p_.held(kHeldEnvelope)) return false;
    uint8_t* row = t.tvas.data() + kTvaPart * part + kTvaKind * kind;
    put8(row, vc2CurveOf(p_.value(Block::kKindB, part, kind, kCurveId)));
    put16(row + 2, vc2DepthOf(p_.value(Block::kKindB, part, kind, kDepthId)));
    return !p_.held(kHeldEnvelope);
}

bool Vc2PartSettings::tvas(Vc2PartRows& t, int part) {
    return everyRow(1, [&](int kind, int) { return tvaRow(t, part, kind) && tvaVelocityRow(t, part, kind); });
}

void Vc2PartSettings::tvaTrailer(Vc2PartRows& t) {
    put8(&t.tvas[kTvaGainAt], p_.value(Block::kSystemC, 0, 0, kTvaTrailerGain));
    put16(&t.tvas[kTvaRateAt], p_.value(Block::kSystemC, 0, 0, kTvaTrailerRate));
}

void Vc2PartSettings::tvaRoute(Vc2PartRows& t) {
    put8(&t.tvas[kTvaRouteAt], p_.value(Block::kSystemB, 0, 0, kTvaRouteId));
}

void Vc2PartSettings::tvaControls(Vc2PartRows& t) { put8(&t.controls, p_.value(Block::kSystem, 0, 0, kControlsId)); }

bool Vc2PartSettings::byteRow(Vc2PartRows& t, int part, int kind) {
    if (p_.held(kHeldKind)) return false;
    put8(&t.bytes[kBytesPart * part + kind], p_.value(Block::kKindA, part, kind, kBytesId));
    return !p_.held(kHeldKind);
}

bool Vc2PartSettings::bytes(Vc2PartRows& t, int part) {
    return everyRow(1, [&](int kind, int) { return byteRow(t, part, kind); });
}

// The COSM's first byte and type, then the type's run of IDs as halfwords from +10.  A type past the table's (the
// parameter's range never gives one) reads nothing: rom.4 reads past its table there, into other data
bool Vc2PartSettings::cosmRow(Vc2PartRows& t, int part, int kind, int cosm) {
    if (p_.held(kHeldCosm)) return false;
    uint8_t* row = t.cosms.data() + kCosmPart * part + kCosmKind * kind + kCosmBytes * cosm;
    put8(row, p_.cosmValue(part, kind, cosm, kCosmFirst));
    const auto type = static_cast<uint8_t>(p_.cosmValue(part, kind, cosm, kCosmTypeId));
    row[1] = type;
    if (type < Vc2PartSettingsTables::kCosmTypes) {
        const auto& runs = vc2PartSettingsTables().cosmRuns;
        for (int id = runs[2 * type], k = 0; id <= runs[2 * type + 1]; ++id, ++k)
            put16(row + kCosmIdsAt + 2 * k, p_.cosmValue(part, kind, cosm, static_cast<int16_t>(id)));
    }
    return !p_.held(kHeldCosm);
}

// Two envelopes' velocity curve and depth, by the laws, into +2…+9; a COSM past the two answers false
bool Vc2PartSettings::cosmVelocityRow(Vc2PartRows& t, int part, int kind, int cosm) {
    if (p_.held(kHeldEnvelope) || static_cast<uint32_t>(cosm) >= kCosms) return false;
    uint8_t* row = t.cosms.data() + kCosmPart * part + kCosmKind * kind + kCosmBytes * cosm;
    for (int k = 0; k < 2; ++k) {
        const int b = kVelocityIds + kEnvelopeStride * kCosmEnvelopes[cosm][k];
        put8(row + 2 + 4 * k, vc2CurveOf(p_.value(Block::kKindB, part, kind, static_cast<int16_t>(b))));
        put16(row + 4 + 4 * k, vc2DepthOf(p_.value(Block::kKindB, part, kind, static_cast<int16_t>(b + 1))));
    }
    return !p_.held(kHeldEnvelope);
}

bool Vc2PartSettings::cosms(Vc2PartRows& t, int part) {
    return everyRow(kCosms, [&](int kind, int cosm) {
        return cosmRow(t, part, kind, cosm) && cosmVelocityRow(t, part, kind, cosm);
    });
}
