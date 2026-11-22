// rom.4's OS MIDI task's constants: see Vc2OsMidiTables.h (v313).  Every address is rom.4's.
#include "Vc2OsMidiTables.h"
#include "Vc2RomTables.h"
#include <algorithm>

namespace {
constexpr uint32_t kPortsAt = 0x8995eaf4, kPortBytes = 16, kPorts = 48;   // [FW] the port objects

struct Read : Vc2OsMidiTables {
    // The three tables; usable when every source's row is one of the rows and every port object is one of the 48
    bool read(Sh4& image) {
        image.read(0x8950d528, portRows.data(), sizeof portRows);
        image.read(0x8950d530, ports.data(), sizeof ports);
        image.read(0x8950d5f0, stateTemplate.data(), sizeof stateTemplate);
        const bool rows = std::ranges::all_of(portRows, [](uint8_t row) { return row < kPortRows; });
        return rows && std::ranges::all_of(ports, [](uint32_t p) {
                   return p == 0 || (p >= kPortsAt && p < kPortsAt + kPortBytes * kPorts && (p - kPortsAt) % 16 == 0);
               });
    }
};
constinit Vc2RomTables<Read> tables;
} // namespace

bool vc2LoadOsMidi(const char* path) { return tables.load(path); }
bool vc2OsMidiLoaded() { return tables.loaded(); }
const Vc2OsMidiTables& vc2OsMidiTables() { return tables.t; }
