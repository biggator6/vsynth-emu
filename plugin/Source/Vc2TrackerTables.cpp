// The VC-2's pitch tracker's constants from rom.4: see Vc2TrackerTables.h (v223).  Every address is rom.4's, each the
// pool word its routine loads.
#include "Vc2TrackerTables.h"
#include "Vc2RomTables.h"
#include <bit>
#include <utility>

namespace {
constexpr int32_t kFrameLength = 881;   // [FW] the table setter's immediate (3 << 8, + 113)

struct Read : Vc2TrackerTables {
    bool read(Sh4& image) {
        Vc2TrackerSettings& s = settings;
        const std::pair<uint32_t, float*> words[] = {   // [FW] each routine's pool
            { 0x893edff4, &rate[0] }, { 0x893edffc, &rate[1] },
            { 0x893ef2e0, &s.mode0.longest }, { 0x893ef2e8, &s.mode0.shortest }, { 0x893ef2f0, &s.follow.longest },
            { 0x893ef2f8, &s.follow.shortest }, { 0x893ef300, &s.mode1.longest }, { 0x893ef308, &s.mode1.shortest },
            { 0x893ef2f0, &s.mode2.longest }, { 0x893ef310, &s.mode2.shortest }, { 0x893ef31c, &s.mode3.longest },
            { 0x893ef324, &s.mode3.shortest }, { 0x893ef32c, &s.periods[0] }, { 0x893ef334, &s.periods[1] },
            { 0x893ef33c, &s.crossingLevel }, { 0x893ef344, &s.levels[0] }, { 0x893ef34c, &s.levels[1] },
            { 0x893ef354, &s.levels[2] }, { 0x893ef35c, &s.levels[3] }, { 0x893ef364, &s.levels[4] },
            { 0x893ef36c, &s.onsetLevel },
            { 0x893ee418, &modes[0].highpass }, { 0x893ee41c, &modes[0].lowpass }, { 0x893ee420, &modes[0].period },
            { 0x893ee3d0, &modes[1].highpass }, { 0x893ee3d8, &modes[1].lowpass }, { 0x893ee3dc, &modes[1].period },
            { 0x893ee3d8, &modes[2].highpass }, { 0x893ee3f4, &modes[2].lowpass }, { 0x893ee3f8, &modes[2].period },
            { 0x893ee404, &modes[3].highpass }, { 0x893ee408, &modes[3].lowpass }, { 0x893ee40c, &modes[3].period },
            { 0x893ee3d0, &highpassStart }, { 0x893ee430, &lowpassStart }, { 0x893ee444, &glide },
            { 0x893ee62c, &half },
        };
        for (const auto& [at, out] : words) *out = std::bit_cast<float>(image.rd32(at));
        s.frameLength = kFrameLength;
        return true;
    }
};

constinit Vc2RomTables<Read> g_tables;
} // namespace

bool vc2LoadTracker(const char* path) { return g_tables.load(path); }
bool vc2TrackerLoaded() { return g_tables.loaded(); }
const Vc2TrackerTables& vc2TrackerTables() { return g_tables.t; }
