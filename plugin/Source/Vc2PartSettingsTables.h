#pragma once
// The part's settings' constants from rom.4 (v287): each COSM type's run of parameter IDs, the first and last, that the
// COSM inputs' reader 0x89399c90 reads, (v288) the OSC part reader's twelve header IDs and (v290) the vocoder
// reader's record floats and pool words, read once by loadVc2 at the start (Vc2RomTables), never after.
// HARDWARE_FIRMWARE.md, "The VC-2 part's settings by ID".
#include <array>
#include <cstdint>

struct Vc2PartSettingsTables {
    static constexpr int kCosmTypes = 17;                  // [FW] the table's types; what follows it is other data
    static constexpr int kRecords = 32;                    // [FW] two a Formant Template, left then right (v290)
    std::array<int16_t, 2 * kCosmTypes> cosmRuns {};       // 0x8953fb40: by type, its first and last ID
    std::array<int16_t, 12> oscIds {};                     // 0x8954012c: the OSC header's twelve (v288)
    // The vocoder reader's (v290): each Formant Template record's third float (0x89540904, 140 bytes a record), and
    // its pool's −64/63, 1/63 and 1/127 (0x893a75ec, 0x893a75f0, 0x893a75f8)
    std::array<float, kRecords> templateShifts {};
    float shiftBase = 0, shiftStep = 0, per127 = 0;
    // The static M's three floats (v295): its +0x288, +0x28c and +0x194, from 0x893a3020's pool (0x893a3098, 0x893a30a0,
    // 0x893a30a4)
    std::array<float, 3> staticFloats {};
};

[[nodiscard]] bool vc2LoadPartSettings(const char* path);
bool vc2PartSettingsLoaded();
const Vc2PartSettingsTables& vc2PartSettingsTables();
