// The VC-2's COSM OD/Dist's and Wave Shaper's converters: see Vc2CosmDrive.h (v239).  In rom.4's order of operations,
// single precision under the SH-4's rounding (XtFloat).
#include "Vc2CosmDrive.h"
#include "Vc2CosmTables.h"

namespace {
constexpr int kMost = 127, kType = 0;                       // [FW] the levels' hold; Type, the first parameter
constexpr int32_t kOdCode = 1, kOdOtherCode = 2, kWsCode = 3;   // [FW]
constexpr int32_t kDriveEnvelope[2] = { 9, 11 }, kToneEnvelope[2] = { 10, 12 };   // [FW] COSM 1's, COSM 2's
// [FW] the sums: the group's 1st, 10th, 2nd and 11th
constexpr int kDriveSum = 0, kDriveLfoSum = 9, kToneSum = 1, kToneLfoSum = 10;

// [FW] a converter's parameters in its input (the Wave Shaper has Drive's Env depth before its LFO depth) and its pool
struct Vc2CosmDriveRow {
    int level, drive, driveLfo, driveEnv, tone, follow, toneLfo, toneEnv;
    Vc2CosmDrivePool Vc2CosmTables::*pool;
};
constexpr Vc2CosmDriveRow kOd { 1, 2, 3, 4, 5, 6, 7, 8, &Vc2CosmTables::od };
constexpr Vc2CosmDriveRow kWs { 1, 2, 4, 3, 5, 6, 7, 8, &Vc2CosmTables::ws };

void driveBody(const Vc2CosmDriveRow& row, const Vc2CosmInput& in, Vc2CosmDriveBody& b, Vc2CosmMatrix& matrix,
               const Vc2ToneShared& shared) {
    const Vc2CosmDrivePool& k = vc2CosmTables().*row.pool;
    const Vc2CosmSums sum { in, matrix };
    b.level = vc2CosmGain(in.params[row.level], k.gainScale);
    b.drive = vc2CosmGain(vc2CosmLevel(in.params[row.drive], sum(kDriveSum), kMost), k.gainScale);
    b.driveMod = vc2CosmModOf(in.params[row.driveEnv], in.velocity[0], kDriveEnvelope[sum.second()],
                              in.params[row.driveLfo], sum(kDriveLfoSum), k.k63);
    const int tone = vc2CosmLevel(in.params[row.tone], sum(kToneSum), kMost);
    b.tone = vc2CosmLevelKeyed(tone, in.noteKey, in.params[row.follow], k.key, shared);
    b.toneMod = vc2CosmModOf(in.params[row.toneEnv], in.velocity[1], kToneEnvelope[sum.second()],
                             in.params[row.toneLfo], sum(kToneLfoSum), k.k63);
}
} // namespace

void vc2OdConvert(const Vc2CosmInput& in, Vc2CosmOdBlock& b, Vc2CosmMatrix& matrix, const Vc2ToneShared& shared) {
    b.code = in.params[kType] == 0 ? kOdCode : kOdOtherCode;
    driveBody(kOd, in, b.body, matrix, shared);
}

void vc2WsConvert(const Vc2CosmInput& in, Vc2CosmWsBlock& b, Vc2CosmMatrix& matrix, const Vc2ToneShared& shared) {
    b.code = kWsCode;
    b.type = static_cast<uint8_t>(in.params[kType]);
    driveBody(kWs, in, b.body, matrix, shared);
}
