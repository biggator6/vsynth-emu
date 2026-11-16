// The VOCODER OSC's constants from rom.4: see Vc2CarrierTables.h (v221, v222).  Every address is rom.4's, each the pool
// word its routine loads.
#include "Vc2CarrierTables.h"
#include "Vc2RomTables.h"
#include <bit>
#include <utility>

namespace {
struct Read : Vc2CarrierTables {
    bool read(Sh4& image) {
        const std::pair<uint32_t, float*> words[] = {   // [FW] each routine's pool
            { 0x893ec6f8, &pitchFactor }, { 0x893ec228, &growlStart }, { 0x893ec22c, &noiseScale },
            { 0x893ec230, &plain.half }, { 0x893ec234, &plain.minusTwo }, { 0x893ec238, &plain.glide },
            { 0x893ec23c, &plain.toBus }, { 0x893ec4b8, &growl.half }, { 0x893ec4ac, &growl.minusTwo },
            { 0x893ec4bc, &growl.glide }, { 0x893ec4c0, &growl.toBus }, { 0x893ec4a0, &overPi },
            { 0x893ec4b0, &k440 }, { 0x893ec4b4, &k50 },
            { 0x893ec6c0, &lowBase }, { 0x893ec6c4, &lowSlope }, { 0x893ec6c8, &minPitch }, { 0x893ec6cc, &highA },
            { 0x893ec6d0, &highB }, { 0x893ec6d4, &highC }, { 0x893ec6d8, &floor }, { 0x893ec6e0, &dynHalf },
            { 0x893ec6e4, &dynLow }, { 0x893ec6e8, &dynSawBase }, { 0x893ec6ec, &dynSaw },
            { 0x893f32ec, &foldAt }, { 0x893f3318, &foldNeg }, { 0x893f3310, &three }, { 0x893f3314, &four },
            { 0x893f3308, &clockScale }, { 0x893f330c, &logistic },
            { 0x8939e0a8, &perLevel }, { 0x8939ddd8, &velocityScale }, { 0x8939ddb4, &sumScale },
            { 0x8939e0b4, &growlScale }, { 0x8939e0b8, &keyfollowScale }, { 0x8939e0c0, &balanceScale },
            { 0x8939c2c0, &depthScale }, { 0x893b0254, &centsMax }, { 0x893b0258, &centsMin },
            { 0x893b025c, &perOctave }, { 0x893b0264, &ratioMax }, { 0x893b0268, &ratioMin },
        };
        for (const auto& [at, out] : words) *out = std::bit_cast<float>(image.rd32(at));
        return true;
    }
};

constinit Vc2RomTables<Read> g_tables;
} // namespace

bool vc2LoadCarrier(const char* path) { return g_tables.load(path); }
bool vc2CarrierLoaded() { return g_tables.loaded(); }
const Vc2CarrierTables& vc2CarrierTables() { return g_tables.t; }
