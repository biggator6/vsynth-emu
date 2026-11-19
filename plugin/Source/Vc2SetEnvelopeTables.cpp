// C's envelopes' constants from rom.4: see Vc2SetEnvelopeTables.h (v282).  Every address is rom.4's.
#include "Vc2SetEnvelopeTables.h"
#include "Vc2RomTables.h"

namespace {
struct Read : Vc2SetEnvelopeTables {
    // The tables and the pool words; usable when the scales are positive and the time of the top value isn't 0
    bool read(Sh4& image) {
        image.read(0x89542134, notes.data(), sizeof notes);
        image.read(0x89541f34, levels.data(), sizeof levels);
        image.read(0x8953fc90, times.data(), sizeof times);
        vc2ReadPool(image, std::to_array<Vc2PoolWord>({ { 0x8939a66c, &noteScale }, { 0x8939a8f4, &releaseScale },
                                                          { 0x8939a67c, &levelScale }, { 0x8939ab50, &cents },
                                                          { 0x8939ab58, &unit } }));
        return noteScale > 0 && releaseScale > 0 && levelScale > 0 && cents > 0 && unit > 0 && times[127] != 0;
    }
};
constinit Vc2RomTables<Read> tables;
} // namespace

bool vc2LoadSetEnvelopes(const char* path) { return tables.load(path); }
bool vc2SetEnvelopesLoaded() { return tables.loaded(); }
const Vc2SetEnvelopeTables& vc2SetEnvelopeTables() { return tables.t; }
