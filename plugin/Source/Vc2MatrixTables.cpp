// rom.4's Matrix tables: see Vc2MatrixTables.h (v235, v245, v247).  Every address is rom.4's.
#include "Vc2MatrixTables.h"
#include "Vc2RomTables.h"
#include <bit>

namespace {
// [FW] the limits, u16 each by destination, and the sources' curve right after them (0x895401f6)
constexpr uint32_t kLimits = 0x89540158;
// [FW] the set-up's destination routines (v247) and the levels' scale
constexpr uint32_t kDestRoutine = 0x893a30d0, kDepthRoutine = 0x893a36a0, kLevelScale = 0x893a3064;
// In the image's interpreter: RAM for the routines' stack and the uses words they set
constexpr uint32_t kScratch = 0x8C000000, kScratchSize = 0x1000, kStack = kScratch + kScratchSize;
constexpr uint64_t kSteps = 10000;   // each routine runs a few dozen instructions
constexpr uint32_t kOn = 1;          // a source and a sensitivity that aren't none

struct Read : Vc2MatrixTables {
    // 0x893a30d0 on a destination: M's destination it returns, and the bit it set in the uses words
    bool readDest(Sh4& image, int d) {
        const uint16_t clear[kGroups] {};
        image.write(kScratch, clear, sizeof clear);
        uint32_t r0 = 0;
        if (!image.call(kDestRoutine, { kOn, (uint32_t)d, kOn, kScratch }, kStack, &r0, kSteps)) return false;
        dests[d].dest = static_cast<uint8_t>(r0);
        for (int g = 0; g < kGroups; ++g) {
            const auto bits = static_cast<uint16_t>(image.rd16(kScratch + 2 * (uint32_t)g));
            if (bits == 0) continue;
            dests[d].group = static_cast<uint8_t>(g);
            dests[d].bit = bits;
        }
        return true;
    }
    // 0x893a36a0 on a destination at every sensitivity
    bool readDepths(Sh4& image, int d) {
        for (int s = -kSensReach; s < kSensReach; ++s) {
            uint32_t r0 = 0;
            if (!image.call(kDepthRoutine, { kOn, (uint32_t)d, (uint32_t)s }, kStack, &r0, kSteps)) return false;
            depths[d][s + kSensReach] = static_cast<int16_t>(r0);
        }
        return true;
    }
    bool read(Sh4& image) {
        uint32_t at = kLimits;
        for (uint16_t& w : words) {
            w = static_cast<uint16_t>(image.rd16(at));
            at += sizeof w;
        }
        levelScale = std::bit_cast<float>(image.rd32(kLevelScale));
        image.mapRam(kScratch, kScratchSize);
        for (int d = 0; d < kDestinations; ++d)
            if (!readDest(image, d) || !readDepths(image, d)) return false;
        return true;
    }
    static constexpr int kGroups = 11;   // [FW] the uses words
};

constinit Vc2RomTables<Read> tables;
} // namespace

bool vc2LoadMatrix(const char* path) { return tables.load(path); }
bool vc2MatrixLoaded() { return tables.loaded(); }
int32_t vc2MatrixLimit(int dest) { return tables.t.words[dest]; }
uint16_t vc2MatrixCurveWord(int i) { return tables.t.words[Vc2MatrixTables::kDestinations + i]; }
Vc2MatrixDest vc2MatrixDestOf(int dest) { return tables.t.dests[dest]; }
int16_t vc2MatrixDepthOf(int dest, int32_t sens) { return tables.t.depths[dest][sens + Vc2MatrixTables::kSensReach]; }
float vc2MatrixLevelScale() { return tables.t.levelScale; }
