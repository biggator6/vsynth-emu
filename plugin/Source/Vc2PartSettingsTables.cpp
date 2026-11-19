// The part's settings' constants from rom.4: see Vc2PartSettingsTables.h (v287).  Every address is rom.4's.
#include "Vc2PartSettingsTables.h"
#include "Vc2RomTables.h"

namespace {
constexpr int kMostIds = 16;   // [FW] a COSM input's row holds 16 halfwords of them

struct Read : Vc2PartSettingsTables {
    // The runs; usable when each type's is in order and fits its row
    bool read(Sh4& image) {
        image.read(0x8953fb40, cosmRuns.data(), sizeof cosmRuns);
        image.read(0x8954012c, oscIds.data(), sizeof oscIds);
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
