// The OSC voice's constants from rom.4: see Vc2OscTables.h (v230, v232).  Every address is rom.4's: a pool word its
// routine loads, or a float its install reads by a pool word's address.
#include "Vc2OscTables.h"
#include "Vc2RomTables.h"

namespace {
// [FW] the update's, the note-on's, the settings', the pitch getter's and the install's; then the targets'; then
// VariPhrase's note-on's (v238) and LITE's (v240)
auto oscWords(Vc2OscTables& t) {
    return std::to_array<Vc2PoolWord>({
        { 0x893ae3d0, &t.waitLevel }, { 0x893ae3f4, &t.depthScale }, { 0x893ae48c, &t.plainHz },
        { 0x893ae628, &t.rateTop }, { 0x893ae630, &t.sampleHz }, { 0x893ae634, &t.cent },
        { 0x893ae638, &t.exponentBase }, { 0x893ae63c, &t.keyCents }, { 0x893ae640, &t.perOctave },
        { 0x893ae644, &t.tuneScale }, { 0x893ae87c, &t.settingsScale }, { 0x893aed7c, &t.getterCents.most },
        { 0x893aed80, &t.getterCents.least }, { 0x89542390, &t.installed4 }, { 0x895422f0, &t.installed5 },
        { 0x895422f4, &t.installed6 }, { 0x893afb38, &t.cents.most }, { 0x893afb3c, &t.cents.least },
        { 0x893afdc0, &t.ratio.perOctave }, { 0x893afdc8, &t.ratio.ratio.most }, { 0x893afdcc, &t.ratio.ratio.least },
        { 0x893afdd4, &t.ratio.cents.most }, { 0x893afdd8, &t.ratio.cents.least }, { 0x893af35c, &t.noteOn.cents.most },
        { 0x893af360, &t.noteOn.cents.least }, { 0x893af364, &t.noteOn.perOctave },
        { 0x893af36c, &t.noteOn.ratio.most }, { 0x893af370, &t.noteOn.ratio.least }, { 0x893af530, &t.kind6Least },
        { 0x893afddc, &t.walkTarget.most }, { 0x893afde0, &t.walkTarget.least }, { 0x893affd4, &t.walk.most },
        { 0x893affd8, &t.walk.least }, { 0x893afde4, &t.formantTargetLeast }, { 0x893affe0, &t.formantLeast },
        { 0x893af53c, &t.timesTenThousand }, { 0x893af7c8, &t.rateTenths }, { 0x893c7d78, &t.liteTimes },
        { 0x893c7f64, &t.liteTenths }, { 0x893c81ec, &t.liteHeadsTenths }, { 0x893c81e4, &t.liteTwo32 },
        { 0x893c81f0, &t.litePeriod }, { 0x893c81f4, &t.liteHalf }, { 0x893c81f8, &t.liteFilter },
    });
}
// [FW] the analog note-on's (v242): its head's, LA's, the other codes', SQUARE's steps', NOISE's and 0x893c4a20's
auto analogWords(Vc2OscAnalogPool& a) {
    auto& [sq, saw] = a.la;
    auto& [st1, st2, st3] = a.squareSteps;
    return std::to_array<Vc2PoolWord>({
        { 0x893c23f4, &a.impactPeriod }, { 0x893c23f8, &a.impactBase }, { 0x893c25d0, &a.laHalf },
        { 0x893c25d4, &a.laFatMin }, { 0x893c25d8, &a.laFatFloor }, { 0x893c25dc, &a.laPwLo },
        { 0x893c25e0, &a.laPwSpan }, { 0x893c25e4, &sq.hz }, { 0x893c25ec, &sq.step }, { 0x893c2604, &saw.hz },
        { 0x893c2608, &saw.step }, { 0x893c25f0, &a.toneScale }, { 0x893c25f4, &a.toneOctaves },
        { 0x893c25f8, &a.corner }, { 0x893c2600, &a.kfRatio }, { 0x893c260c, &a.cornerMost },
        { 0x893c2870, &a.gainScale }, { 0x893c2878, &a.step }, { 0x893c2880, &a.fullScale },
        { 0x893c2884, &a.phaseStart }, { 0x893c2888, &a.half }, { 0x893c288c, &a.four }, { 0x893c2890, &a.wrap },
        { 0x893c2894, &a.fatMin }, { 0x893c2898, &a.pwLo }, { 0x893c289c, &a.pwSpan }, { 0x893c2af4, &a.sawSlope },
        { 0x893c2af8, &a.sawSpan }, { 0x893c2afc, &a.sawLo }, { 0x893c2b00, &a.ramp.lo }, { 0x893c2b04, &a.ramp.span },
        { 0x893c2b08, &a.square.lo }, { 0x893c2b0c, &a.square.span }, { 0x893c2b34, &a.triangle.lo },
        { 0x893c2b38, &a.triangle.span }, { 0x893c2b3c, &a.juno.lo }, { 0x893c2b40, &a.juno.span },
        { 0x893c2b10, &a.squareLeast }, { 0x893c2b14, &a.squareFirst }, { 0x893c2b2c, &a.squareTop },
        { 0x893c2b30, &a.edgeScale }, { 0x893c2b18, &st1.bound }, { 0x893c2b1c, &st1.step },
        { 0x893c2b20, &st2.bound }, { 0x893c2b24, &st2.step }, { 0x893c2b28, &st3.bound }, { 0x893c2af8, &st3.step },
        { 0x893c2b44, &a.noiseSpan }, { 0x893c4a94, &a.withinHalf }, { 0x893c3c20, &a.sub.fullScale },
        { 0x893c3c24, &a.sub.step }, { 0x893c3c28, &a.sub.levelStep }, { 0x893c3e7c, &a.sub.half },
        { 0x893c3e80, &a.sub.wrap },
    });
}
// [FW] the sub oscillator's shape's (v246): its head's; LA-SQUARE's and LA-SAW's, each case's own
auto shapeWords(Vc2OscShapePool& s) {
    auto& [sq, saw] = s.la;
    return std::to_array<Vc2PoolWord>({
        { 0x893c41f0, &s.head.noiseHalf }, { 0x893c41f4, &s.head.noiseSpan }, { 0x893c41f8, &s.head.gainScale },
        { 0x893c41fc, &s.head.levelStep }, { 0x893c4a70, &sq.step },
        { 0x893c4a78, &sq.hz440 }, { 0x893c4a7c, &sq.toneScale }, { 0x893c4a80, &sq.toneOctaves },
        { 0x893c4a84, &sq.corner }, { 0x893c4a8c, &sq.kf1 }, { 0x893c4a90, &sq.most }, { 0x893c4a94, &sq.half },
        { 0x893c4a98, &sq.hz220 }, { 0x893c4a9c, &sq.kf2 }, { 0x893c4aa0, &sq.floor }, { 0x893c480c, &saw.step },
        { 0x893c47f8, &saw.hz440 }, { 0x893c4818, &saw.toneScale }, { 0x893c481c, &saw.toneOctaves },
        { 0x893c4820, &saw.corner }, { 0x893c4828, &saw.kf1 }, { 0x893c4828, &saw.kf2 }, { 0x893c482c, &saw.most },
        { 0x893c4804, &saw.half }, { 0x893c4830, &saw.hz220 }, { 0x893c4808, &saw.floor },
    });
}
// [FW] the shape's waves' words (v246), codes 2…7, each where its case reads it, in Vc2OscShapeWave's order: the
// line's least and span, the Hz, the rate, the holds' scale and ½, 0.002, the step, the end's word; 0 where the case
// reads none
struct ShapeWaveAt {
    uint32_t lo, span, hz, fs, scale, half, floor, step, end;
};
constexpr std::array<ShapeWaveAt, 6> kShapeWaves { {
    { 0x893c4514, 0x893c4518, 0x893c451c, 0x893c4520, 0x893c4524, 0x893c4a94, 0x893c4528, 0x893c4510, 0 },
    { 0x893c452c, 0x893c4530, 0x893c4538, 0x893c4520, 0x893c4534, 0x893c4a94, 0x893c4528, 0x893c4510, 0x893c453c },
    { 0x893c452c, 0x893c4530, 0x893c451c, 0x893c4520, 0x893c4534, 0x893c4a94, 0x893c4528, 0x893c4510, 0 },
    { 0x893c47f0, 0x893c47f4, 0x893c47f8, 0x893c47fc, 0x893c4800, 0x893c4804, 0x893c4808, 0x893c480c, 0x893c4810 },
    { 0, 0, 0x893c451c, 0x893c4520, 0x893c4534, 0x893c4a94, 0x893c4528, 0x893c4510, 0 },
    { 0x893c4540, 0x893c4544, 0x893c4538, 0x893c4520, 0x893c4534, 0x893c4a94, 0x893c4528, 0x893c4510, 0x893c453c },
} };

void readShapeWave(Sh4& image, const ShapeWaveAt& at, Vc2OscShapeWave& w) {
    const auto words = std::to_array<Vc2PoolWord>({
        { at.lo, &w.line.lo }, { at.span, &w.line.span }, { at.hz, &w.hz }, { at.fs, &w.fs }, { at.scale, &w.scale },
        { at.half, &w.half }, { at.floor, &w.floor }, { at.step, &w.step }, { at.end, &w.end },
    });
    for (const auto& [word, out] : words) {
        if (word != 0) *out = std::bit_cast<float>(image.rd32(word));
    }
}

void readShapeWaves(Sh4& image, std::array<Vc2OscShapeWave, kShapeWaves.size()>& waves) {
    auto wave = waves.begin();
    for (const ShapeWaveAt& at : kShapeWaves) readShapeWave(image, at, *wave++);
}
// [FW] the players' installs' tables (v238); the Tone Keyfollow's (v242: rom.2's 15, its [3] 0 and [14] 2), and the
// shape's copy (v246, the same 15, other words past them)
constexpr Vc2Span kLiteInstalled { 0x89542350, 6 }, kVariInstalled { 0x89542368, 10 }, kToneKf { 0x89546fbc, 15 };
constexpr Vc2Span kShapeToneKf { 0x89560064, 15 };

struct Read : Vc2OscTables {
    bool read(Sh4& image) {
        vc2ReadPool(image, oscWords(*this));
        vc2ReadFloats(image, kLiteInstalled, liteInstalled.data());
        vc2ReadFloats(image, kVariInstalled, variInstalled.data());
        vc2ReadPool(image, analogWords(analog));
        vc2ReadFloats(image, kToneKf, analog.toneKf.data());
        vc2ReadPool(image, shapeWords(analog.shape));
        readShapeWaves(image, analog.shape.waves);
        vc2ReadFloats(image, kShapeToneKf, analog.shape.toneKf.data());
        return true;
    }
};

constinit Vc2RomTables<Read> tables;
} // namespace

bool vc2LoadOsc(const char* path) { return tables.load(path); }
bool vc2OscLoaded() { return tables.loaded(); }
const Vc2OscTables& vc2OscTables() { return tables.t; }
