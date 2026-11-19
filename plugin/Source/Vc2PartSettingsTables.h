#pragma once
// The part's settings' constants from rom.4 (v287): each COSM type's run of parameter IDs, the first and last, that the
// COSM inputs' reader 0x89399c90 reads, read once by loadVc2 at the start (Vc2RomTables), never after.
// HARDWARE_FIRMWARE.md, "The VC-2 part's settings by ID".
#include <array>
#include <cstdint>

struct Vc2PartSettingsTables {
    static constexpr int kCosmTypes = 17;                  // [FW] the table's types; what follows it is other data
    std::array<int16_t, 2 * kCosmTypes> cosmRuns {};       // 0x8953fb40: by type, its first and last ID
};

[[nodiscard]] bool vc2LoadPartSettings(const char* path);
bool vc2PartSettingsLoaded();
const Vc2PartSettingsTables& vc2PartSettingsTables();
