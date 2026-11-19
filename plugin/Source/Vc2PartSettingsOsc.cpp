// The VC-2 part's settings by ID, the OSC table's readers (v288): see Vc2PartSettings.h.  Each reader's flag tests,
// reads and stores in rom.4's order.
#include "Vc2PartSettings.h"
#include "Vc2PartSettingsTables.h"
#include "Vc2PartSettingsWords.h"

using namespace Vc2PartWords;

namespace {
using Block = Vc2ParamBlock;
constexpr int kOscs = 2;                                          // [FW] a kind's OSCs
// [FW] the table: a part's row, its header, a kind's and an OSC's blocks; the systems' trailer and calibration
constexpr uint32_t kOscPart = 0x1020, kBlocksAt = 32, kOscKind = 0x100, kOscBytes = 0x80;
constexpr uint32_t kTrailerAt = 0x10200, kCalibrationAt = 0x10204;
// [FW] the header's IDs: +0 (× 12), +1, +2 (× 10), +4 (whether 0), +5, the twelve's switch, +0x1e
constexpr int16_t kHeaderIds[] = { 0x12f, 0x12d, 0x12e, 0x130, 0x133 }, kTwelveOn = 0x151, kHeaderLast = 0x193;
constexpr uint32_t kTwelveAt = 6, kHeaderLastAt = 0x1e;
constexpr int32_t kOctave = 12, kTen = 10;
// [FW] the trailer's (the second system block): a halfword, then two bytes
constexpr int16_t kTrailerIds[] = { 62, 63, 122 };
// [FW] the calibration's (the third system block): +0 cleared, five bytes, each from 0x534 + its place, read in this
// order, two halfwords, its groups' switch, then four groups of eleven bytes four apart from consecutive IDs, or each
// group's value when off
constexpr int16_t kCalibrationIds = 0x534, kHalfwordIds = 0x53a, kGroupsOn = 0x53c, kGroupIds = 0x53d;
constexpr uint32_t kCalibrationBytes[] = { 1, 3, 2, 4, 5 };
constexpr int kGroups = 4, kGroupLength = 11, kGroupStride = 4;
constexpr uint32_t kGroupAt[kGroups] = { 0xa, 0xc, 0xb, 0xd };
constexpr uint8_t kGroupOff[kGroups] = { 60, 0, 0, 127 };
// [FW] each OSC block's six envelopes whose velocity curve and depth it takes (OSC 1's four on), and where they go
constexpr int kVelocityEnvelopes[] = { 1, 2, 3, 2, 3, 4 }, kOscEnvelopeStep = 4;
constexpr uint32_t kCurveAt[] = { 0xa, 0x28, 0x32, 0x4a, 0x54, 0x19 };
constexpr uint32_t kDepthAt[] = { 0xc, 0x2a, 0x34, 0x4c, 0x56, 0x1a };
} // namespace

void Vc2PartSettings::oscTrailer(Vc2PartRows& t) {
    uint8_t* u = t.oscs.data() + kTrailerAt;
    put16(u, p_.value(Block::kSystemB, 0, 0, kTrailerIds[0]));
    put8(u + 2, p_.value(Block::kSystemB, 0, 0, kTrailerIds[1]));
    put8(u + 3, p_.value(Block::kSystemB, 0, 0, kTrailerIds[2]));
}

void Vc2PartSettings::oscCalibration(Vc2PartRows& t) {
    uint8_t* u = t.oscs.data() + kCalibrationAt;
    const auto get = [&](int at) { return p_.value(Block::kSystemC, 0, 0, static_cast<int16_t>(at)); };
    u[0] = 0;
    for (const uint32_t b : kCalibrationBytes) put8(u + b, get(kCalibrationIds + static_cast<int>(b)));
    put16(u + 6, get(kHalfwordIds));
    put16(u + 8, get(kHalfwordIds + 1));
    const bool on = get(kGroupsOn) != 0;
    for (int g = 0; g < kGroups; ++g)
        for (int k = 0; k < kGroupLength; ++k)
            put8(u + kGroupAt[g] + kGroupStride * k, on ? get(kGroupIds + kGroupLength * g + k) : kGroupOff[g]);
}

void Vc2PartSettings::oscHeader(uint8_t* row, int part) {
    const auto get = [&](int16_t id) { return p_.value(Block::kPart, part, 0, id); };
    put8(row, times(get(kHeaderIds[0]), kOctave));
    put8(row + 1, get(kHeaderIds[1]));
    put16(row + 2, times(get(kHeaderIds[2]), kTen));
    put8(row + 4, get(kHeaderIds[3]) == 0 ? 1 : 0);
    put8(row + 5, get(kHeaderIds[4]));
    const bool twelve = get(kTwelveOn) != 0;
    const auto& ids = vc2PartSettingsTables().oscIds;
    for (size_t k = 0; k < ids.size(); ++k) put16(row + kTwelveAt + 2 * k, twelve ? times(get(ids[k]), kTen) : 0);
    put8(row + kHeaderLastAt, get(kHeaderLast));
}

bool Vc2PartSettings::oscVelocityRow(uint8_t* block, int part, int kind, int osc) {
    if (p_.held(kHeldEnvelope)) return false;
    for (size_t k = 0; k < std::size(kVelocityEnvelopes); ++k) {
        const int envelope = kVelocityEnvelopes[k] + kOscEnvelopeStep * osc;
        velocityPair(block + kCurveAt[k], block + kDepthAt[k], part, kind, envelope);
    }
    return !p_.held(kHeldEnvelope);
}

bool Vc2PartSettings::oscs(Vc2PartRows& t, int part) {
    if (p_.held(kHeldCommon)) return false;
    uint8_t* row = t.oscs.data() + kOscPart * part;
    oscHeader(row, part);
    if (p_.held(kHeldCommon)) return false;
    return everyRow(kOscs, [&](int kind, int osc) {
        uint8_t* block = row + kBlocksAt + kOscKind * kind + kOscBytes * osc;
        return p_.oscBlock(part, kind, osc) && oscVelocityRow(block, part, kind, osc);
    });
}
