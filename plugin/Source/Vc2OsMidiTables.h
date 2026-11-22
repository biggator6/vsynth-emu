#pragma once
// rom.4's OS MIDI task's constants (v313): each source's row of port objects, each row's 16 port objects by address,
// and a channel state's template, read once by loadVc2 at the start (Vc2RomTables), never after.  The channel states'
// own table is v276's (Vc2KeysTables::states).  HARDWARE_FIRMWARE.md, "rom.4's MIDI channel states".
#include <array>
#include <cstdint>

struct Vc2OsMidiTables {
    static constexpr int kSources = 5, kPortRows = 4, kTemplateBytes = 0xe8;
    std::array<uint8_t, kSources> portRows {};                // 0x8950d528: each source's row of port objects
    std::array<uint32_t, kPortRows * 16> ports {};            // 0x8950d530: each row's 16, by address (0 none)
    std::array<uint8_t, kTemplateBytes> stateTemplate {};     // 0x8950d5f0: a channel state from its +2
};

[[nodiscard]] bool vc2LoadOsMidi(const char* path);
bool vc2OsMidiLoaded();
const Vc2OsMidiTables& vc2OsMidiTables();
