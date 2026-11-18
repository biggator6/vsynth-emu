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
        { 0x893c6420, &t.variOffScale }, { 0x893c6764, &t.variOffFold }, { 0x893c6424, &t.variOffGlide },
        { 0x893c6428, &t.variOffFade },
    });
}
// [FW] each word where its routine reads it; an address of 0 is a word the routine doesn't read, left 0
void readWords(Sh4& image, std::span<const Vc2PoolWord> words) {
    for (const auto& [word, out] : words) {
        if (word != 0) *out = std::bit_cast<float>(image.rd32(word));
    }
}

// [FW] the analog note-on's head's and phases' own (v242), 0x893c4a20's and the sub oscillator's note-on's (v244)
auto analogWords(Vc2OscAnalogPool& a) {
    return std::to_array<Vc2PoolWord>({
        { 0x893c23f4, &a.impactPeriod }, { 0x893c23f8, &a.impactBase }, { 0x893c2880, &a.fullScale },
        { 0x893c2884, &a.phaseStart }, { 0x893c288c, &a.four }, { 0x893c2890, &a.wrap },
        { 0x893c4a94, &a.withinHalf }, { 0x893c3c20, &a.sub.fullScale }, { 0x893c3c24, &a.sub.step },
        { 0x893c3c28, &a.sub.levelStep }, { 0x893c3e7c, &a.sub.half }, { 0x893c3e80, &a.sub.wrap },
    });
}

// [FW] the plain codes' words where each routine reads them, in Vc2OscCodeWords' order (0: none): ½, Fat's 70%, least
// and floor, the Pulse Width's line, SAW's three; RAMP's, SQUARE's, TRIANGLE's and JUNO's lines (least, span); SQUARE's
// least, first, top and scale, its steps (bound, step); NOISE's 30
struct CodeWordsAt {
    uint32_t half, fatMove, fatMin, fatFloor, pwLo, pwSpan, sawSlope, sawSpan, sawLo;
    std::array<uint32_t, 8> lines;
    uint32_t squareLeast, squareFirst, squareTop, edgeScale;
    std::array<uint32_t, 6> steps;
    uint32_t noiseSpan;
};
// the note-on's (v242; its floor the shapes' 0.002, SQUARE's last step SAW's span) and the update's (v248)
constexpr CodeWordsAt kNoteOnCodes { 0x893c2888, 0, 0x893c2894, 0x893c2878, 0x893c2898, 0x893c289c, 0x893c2af4,
                                     0x893c2af8, 0x893c2afc,
                                     { 0x893c2b00, 0x893c2b04, 0x893c2b08, 0x893c2b0c, 0x893c2b34, 0x893c2b38,
                                       0x893c2b3c, 0x893c2b40 },
                                     0x893c2b10, 0x893c2b14, 0x893c2b2c, 0x893c2b30,
                                     { 0x893c2b18, 0x893c2b1c, 0x893c2b20, 0x893c2b24, 0x893c2b28, 0x893c2af8 },
                                     0x893c2b44 };
constexpr CodeWordsAt kUpdateCodes { 0x893c2e74, 0x893c2e78, 0x893c2e7c, 0x893c2e80, 0x893c2e84, 0x893c2e88, 0x893c3068,
                                     0x893c306c, 0x893c3070,
                                     { 0x893c3060, 0x893c3064, 0x893c3074, 0x893c3078, 0x893c30a0, 0x893c30a4,
                                       0x893c30a8, 0x893c30ac },
                                     0x893c307c, 0x893c3080, 0x893c3098, 0x893c309c,
                                     { 0x893c3084, 0x893c3088, 0x893c308c, 0x893c3090, 0x893c3094, 0x893c306c },
                                     0x893c30b0 };

void readCodeWords(Sh4& image, const CodeWordsAt& at, Vc2OscCodeWords& w) {
    auto& [s1, s2, s3] = w.squareSteps;
    const auto& [rLo, rSpan, sLo, sSpan, tLo, tSpan, jLo, jSpan] = at.lines;
    const auto& [b1, t1, b2, t2, b3, t3] = at.steps;
    readWords(image, std::to_array<Vc2PoolWord>({
        { at.half, &w.half }, { at.fatMove, &w.fatMove }, { at.fatMin, &w.fatMin }, { at.fatFloor, &w.fatFloor },
        { at.pwLo, &w.pwLo }, { at.pwSpan, &w.pwSpan }, { at.sawSlope, &w.sawSlope }, { at.sawSpan, &w.sawSpan },
        { at.sawLo, &w.sawLo }, { rLo, &w.ramp.lo }, { rSpan, &w.ramp.span }, { sLo, &w.square.lo },
        { sSpan, &w.square.span }, { tLo, &w.triangle.lo }, { tSpan, &w.triangle.span }, { jLo, &w.juno.lo },
        { jSpan, &w.juno.span }, { at.squareLeast, &w.squareLeast }, { at.squareFirst, &w.squareFirst },
        { at.squareTop, &w.squareTop }, { at.edgeScale, &w.edgeScale }, { b1, &s1.bound }, { t1, &s1.step },
        { b2, &s2.bound }, { t2, &s2.step }, { b3, &s3.bound }, { t3, &s3.step }, { at.noiseSpan, &w.noiseSpan },
    }));
}

// [FW] LA's words where each routine reads them, in Vc2OscLaWords' order: Fat's ½, least and floor, the Pulse Width's
// line, LA-SQUARE's and LA-SAW's Hz and step, the Tone's three, the keyfollow's ratio, the most, the gain's, the step
struct LaWordsAt {
    uint32_t half, fatMin, fatFloor, pwLo, pwSpan;
    std::array<uint32_t, 4> rows;
    uint32_t toneScale, toneOctaves, corner, kfRatio, cornerMost, gainScale, step;
};
// the note-on's (0x893c2404…, v242) and the update's (0x893c31d0, v248)
constexpr LaWordsAt kNoteOnLa { 0x893c25d0, 0x893c25d4, 0x893c25d8, 0x893c25dc, 0x893c25e0,
                                { 0x893c25e4, 0x893c25ec, 0x893c2604, 0x893c2608 },
                                0x893c25f0, 0x893c25f4, 0x893c25f8, 0x893c2600, 0x893c260c, 0x893c2870, 0x893c2878 };
constexpr LaWordsAt kUpdateLa { 0x893c338c, 0x893c33a0, 0x893c3390, 0x893c33a4, 0x893c33a8,
                                { 0x893c33ac, 0x893c33b0, 0x893c33cc, 0x893c33d0 },
                                0x893c33b8, 0x893c33bc, 0x893c33c0, 0x893c33c8, 0x893c33d4, 0x893c34f4, 0x893c34fc };

void readLaWords(Sh4& image, const LaWordsAt& at, Vc2OscLaWords& w) {
    auto& [sq, saw] = w.la;
    const auto& [sqHz, sqStep, sawHz, sawStep] = at.rows;
    readWords(image, std::to_array<Vc2PoolWord>({
        { at.half, &w.half }, { at.fatMin, &w.fatMin }, { at.fatFloor, &w.fatFloor }, { at.pwLo, &w.pwLo },
        { at.pwSpan, &w.pwSpan }, { sqHz, &sq.hz }, { sqStep, &sq.step }, { sawHz, &saw.hz }, { sawStep, &saw.step },
        { at.toneScale, &w.toneScale }, { at.toneOctaves, &w.toneOctaves }, { at.corner, &w.corner },
        { at.kfRatio, &w.kfRatio }, { at.cornerMost, &w.cornerMost }, { at.gainScale, &w.gainScale },
        { at.step, &w.step },
    }));
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
    readWords(image, std::to_array<Vc2PoolWord>({
        { at.lo, &w.line.lo }, { at.span, &w.line.span }, { at.hz, &w.hz }, { at.fs, &w.fs }, { at.scale, &w.scale },
        { at.half, &w.half }, { at.floor, &w.floor }, { at.step, &w.step }, { at.end, &w.end },
    }));
}

void readShapeWaves(Sh4& image, std::array<Vc2OscShapeWave, kShapeWaves.size()>& waves) {
    auto wave = waves.begin();
    for (const ShapeWaveAt& at : kShapeWaves) readShapeWave(image, at, *wave++);
}
// [FW] the players' installs' tables (v238); the Tone Keyfollow's (v242: rom.2's 15, its [3] 0 and [14] 2), and the
// shape's copy (v246, the same 15, other words past them)
constexpr Vc2Span kLiteInstalled { 0x89542350, 6 }, kVariInstalled { 0x89542368, 10 }, kToneKf { 0x89546fbc, 15 };
constexpr Vc2Span kShapeToneKf { 0x89560064, 15 };
// [FW] the analog's installs' floats by code (v248): the install's, the sub oscillator's install's
constexpr Vc2Span kAnalogInstalled { 0x895422f8, kVc2AnalogCodes }, kSubInstalled { 0x89542324, kVc2AnalogCodes };

struct Read : Vc2OscTables {
    bool read(Sh4& image) {
        vc2ReadPool(image, oscWords(*this));
        vc2ReadFloats(image, kLiteInstalled, liteInstalled.data());
        vc2ReadFloats(image, kVariInstalled, variInstalled.data());
        vc2ReadPool(image, analogWords(analog));
        readCodeWords(image, kNoteOnCodes, analog.codes);
        readCodeWords(image, kUpdateCodes, analog.updateCodes);
        readLaWords(image, kNoteOnLa, analog.la);
        readLaWords(image, kUpdateLa, analog.updateLa);
        vc2ReadFloats(image, kToneKf, analog.toneKf.data());
        vc2ReadFloats(image, kAnalogInstalled, analog.installed.data());
        vc2ReadFloats(image, kSubInstalled, analog.subInstalled.data());
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
