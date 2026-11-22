// The OSC block's converter's constants from rom.4: see Vc2OscConvertTables.h (v251).  Every address is rom.4's, each
// the pool word its routine loads.
#include "Vc2OscConvertTables.h"
#include "Vc2RomTables.h"

namespace {
constexpr uint32_t kCentsAt = 0x89540114, kLevelAt = 0x89541f34;   // [FW] the two curves
constexpr uint32_t kLevelBefore = 128;                             // [FW] the signed index's reach below the curve

// [FW] each routine's pool
auto convertWords(Vc2OscConvertTables& t) {
    return std::to_array<Vc2PoolWord>({
        { 0x8939d07c, &t.perTenth }, { 0x8939d084, &t.centsMost }, { 0x8939d310, &t.centsLeast },
        { 0x8939d31c, &t.depthMost }, { 0x8939d324, &t.depthLeast }, { 0x8939d320, &t.sumScale },
        { 0x8939d328, &t.perOctave }, { 0x8939d330, &t.beamScale }, { 0x8939d340, &t.velocityScale },
        { 0x8939d350, &t.gainScale }, { 0x8939c2dc, &t.tableTop },
        { 0x8939c2e0, &t.lines[0].top }, { 0x8939c2e4, &t.lines[0].offset }, { 0x8939c2e8, &t.lines[0].slope },
        { 0x8939c2ec, &t.lines[1].top }, { 0x8939c2f0, &t.lines[1].offset }, { 0x8939c2f4, &t.lines[1].slope },
        { 0x8939c2f8, &t.lines[2].top }, { 0x8939c2fc, &t.lines[2].offset }, { 0x8939c300, &t.lines[2].slope },
        { 0x8939c304, &t.lines[3].offset }, { 0x8939c308, &t.lines[3].slope },
        { 0x8939cb24, &t.kfScale }, { 0x8939cb2c, &t.levelScale },
    });
}

// [FW] the analog converter's second part's (v256)
auto secondPartWords(Vc2OscConvertTables& t) {
    return std::to_array<Vc2PoolWord>({
        { 0x8939c814, &t.widthSteps }, { 0x8939c820, &t.detuneKeyScale }, { 0x8939c800, &t.sumLeast },
        { 0x8939d890, &t.subLowest }, { 0x8939d634, &t.impactScale }, { 0x8939d638, &t.impactPer },
        { 0x8939cd7c, &t.slopeScale }, { 0x8939c824, &t.detune[0].top },
        { 0x8939c828, &t.detune[1].top }, { 0x8939c82c, &t.detune[1].offset },
        { 0x8939c830, &t.detune[2].top }, { 0x8939c834, &t.detune[2].offset }, { 0x8939c838, &t.detune[2].slope },
        { 0x8939c83c, &t.detune[3].top }, { 0x8939c840, &t.detune[3].offset }, { 0x8939c844, &t.detune[3].slope },
        { 0x8939c848, &t.detune[4].offset }, { 0x8939c84c, &t.detune[4].slope },
    });
}

// [FW] EXTIN's converter's and its formant law's (v393)
auto extinWords(Vc2OscConvertTables& t) {
    return std::to_array<Vc2PoolWord>({
        { 0x8939edf8, &t.extinGainScale }, { 0x8939edec, &t.extinVelocityScale }, { 0x8939edc4, &t.extinDepthTop },
        { 0x8939c59c, &t.formantStep }, { 0x8939c560, &t.formantCents }, { 0x8939c56c, &t.formantMost },
        { 0x8939c568, &t.formantDepth },
    });
}
constexpr uint32_t kFormantRowsAt = 0x8993d0c8;   // [FW] .data, as C startup leaves it

struct Read : Vc2OscConvertTables {
    bool read(Sh4& image) {
        vc2ReadPool(image, convertWords(*this));
        vc2ReadPool(image, secondPartWords(*this));
        vc2ReadPool(image, extinWords(*this));
        image.read(kFormantRowsAt, formantRows.data(), sizeof formantRows);
        detune[0].slope = 1.0f;   // [FW] 0x8939c744: the first line is x itself (+ fldi0's 0)
        detune[1].slope = 2.0f;   // [FW] 0x8939c75a: fldi1, doubled
        for (uint32_t i = 0; i < cents.size(); ++i) cents[i] = image.rd8(kCentsAt + i);
        for (uint32_t i = 0; i < level.size(); ++i) level[i] = image.rd16(kLevelAt + 2 * (i - kLevelBefore));
        return true;
    }
};

constinit Vc2RomTables<Read> tables;
} // namespace

bool vc2LoadOscConvert(const char* path) { return tables.load(path); }
bool vc2OscConvertLoaded() { return tables.loaded(); }
const Vc2OscConvertTables& vc2OscConvertTables() { return tables.t; }
