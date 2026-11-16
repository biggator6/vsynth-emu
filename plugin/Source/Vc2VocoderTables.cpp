// The vocoder object's tables from rom.4: see Vc2VocoderTables.h (v220).  Every address is rom.4's.
#include "Vc2VocoderTables.h"
#include "Vc2RomTables.h"
#include <bit>
#include <utility>

namespace {
constexpr uint32_t kEditAt = 0x8994025c;   // [FW] .data, the edit page's words
// [FW] the pools' words, each the constant the routine named loads
constexpr uint32_t kBuildGain = 0x893f0630, kGainScale = 0x893f063c, kGlide = 0x893f08ec, kMinRatio = 0x893f08f8;
constexpr uint32_t kPeriodA440 = 0x893f09f8;   // [FW] the static initializer 0x893f09b0's, into 0x899d9d90

struct Read : Vc2VocoderTables {
    bool read(Sh4& image) {
        const Vc2PoolWord words[] = {
            { kBuildGain, &buildGain }, { kGainScale, &gainScale }, { kGlide, &glide }, { kMinRatio, &minRatio },
            { kPeriodA440, &periodA440 },
        };
        vc2ReadPool(image, words);
        uint32_t raw[sizeof edit / 4];
        for (uint32_t i = 0; i < sizeof edit / 4; ++i) raw[i] = image.rd32(kEditAt + 4 * i);
        edit = std::bit_cast<Vc2VocoderEdit>(raw);
        return true;
    }
};

constinit Vc2RomTables<Read> g_tables;
} // namespace

bool vc2LoadVocoder(const char* path) { return g_tables.load(path); }
bool vc2VocoderLoaded() { return g_tables.loaded(); }
const Vc2VocoderTables& vc2VocoderTables() { return g_tables.t; }
