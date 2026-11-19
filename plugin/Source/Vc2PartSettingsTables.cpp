// The part's settings' constants from rom.4: see Vc2PartSettingsTables.h (v287).  Every address is rom.4's.
#include "Vc2PartSettingsTables.h"
#include "Vc2RomTables.h"

namespace {
constexpr int kMostIds = 16;   // [FW] a COSM input's row holds 16 halfwords of them
// [FW] the Formant Template records, 140 bytes each, the float the vocoder reader takes their third
constexpr uint32_t kRecordsAt = 0x89540904, kRecordBytes = 140, kShiftAt = 8;

struct Read : Vc2PartSettingsTables {
    // The runs; usable when each type's is in order and fits its row
    bool read(Sh4& image) {
        image.read(0x8953fb40, cosmRuns.data(), sizeof cosmRuns);
        image.read(0x8954012c, oscIds.data(), sizeof oscIds);
        for (uint32_t r = 0; r < kRecords; ++r)
            templateShifts[r] = std::bit_cast<float>(image.rd32(kRecordsAt + kRecordBytes * r + kShiftAt));
        const Vc2PoolWord pool[] = { { 0x893a75ec, &shiftBase }, { 0x893a75f0, &shiftStep }, { 0x893a75f8, &per127 } };
        vc2ReadPool(image, pool);
        for (int t = 0; t < kCosmTypes; ++t) {
            const int first = cosmRuns[2 * t], last = cosmRuns[2 * t + 1];
            if (last < first || last - first >= kMostIds) return false;
        }
        return oscIds[0] != 0;
    }
};
constinit Vc2RomTables<Read> tables;
} // namespace

bool vc2LoadPartSettings(const char* path) { return tables.load(path); }
bool vc2PartSettingsLoaded() { return tables.loaded(); }
const Vc2PartSettingsTables& vc2PartSettingsTables() { return tables.t; }
