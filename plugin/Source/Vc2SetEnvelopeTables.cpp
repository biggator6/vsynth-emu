// C's envelopes' constants from rom.4: see Vc2SetEnvelopeTables.h (v282).  Every address is rom.4's.
#include "Vc2SetEnvelopeTables.h"
#include "Vc2RomTables.h"

namespace {
struct Read : Vc2SetEnvelopeTables {
    // The tables and the pool words; usable when the scales are positive and the time and rate of the top value aren't 0
    bool read(Sh4& image) {
        image.read(0x89542134, notes.data(), sizeof notes);
        image.read(0x89541f34, levels.data(), sizeof levels);
        image.read(0x8953fc90, times.data(), sizeof times);
        vc2ReadFloats(image, { 0x8953fdb0, static_cast<int>(rates.size()) }, rates.data());
        vc2ReadPool(image, std::to_array<Vc2PoolWord>({ { 0x8939a66c, &noteScale }, { 0x8939a8f4, &releaseScale },
                                                          { 0x8939a67c, &levelScale }, { 0x8939ab50, &cents },
                                                          { 0x8939ab58, &unit }, { 0x8939b624, &depthTop },
                                                          { 0x8939b628, &depthScale }, { 0x8939bcf8, &mixOff },
                                                          { 0x8939bf08, &mixOther }, { 0x8939bd00, &mixScale },
                                                          { 0x8939bf04, &mixScaleTwo } }));
        const bool scales = noteScale > 0 && releaseScale > 0 && levelScale > 0 && depthTop > 0 && depthScale > 0;
        const bool mix = mixOff > 0 && mixOther > 0 && mixScale > 0 && mixScaleTwo > 0;
        return scales && mix && cents > 0 && unit > 0 && times[127] != 0 && rates[127] != 0;
    }
};
constinit Vc2RomTables<Read> tables;
} // namespace

bool vc2LoadSetEnvelopes(const char* path) { return tables.load(path); }
bool vc2SetEnvelopesLoaded() { return tables.loaded(); }
const Vc2SetEnvelopeTables& vc2SetEnvelopeTables() { return tables.t; }
