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
// [FW] the players' installs' tables (v238); the Tone Keyfollow's (v242: rom.2's 15, its [3] 0 and [14] 2)
constexpr Vc2Span kLiteInstalled { 0x89542350, 6 }, kVariInstalled { 0x89542368, 10 }, kToneKf { 0x89546fbc, 15 };

struct Read : Vc2OscTables {
    bool read(Sh4& image) {
        vc2ReadPool(image, oscWords(*this));
        vc2ReadFloats(image, kLiteInstalled, liteInstalled.data());
        vc2ReadFloats(image, kVariInstalled, variInstalled.data());
        vc2ReadPool(image, analogWords(analog));
        vc2ReadFloats(image, kToneKf, analog.toneKf.data());
        return true;
    }
};

constinit Vc2RomTables<Read> tables;
} // namespace

bool vc2LoadOsc(const char* path) { return tables.load(path); }
bool vc2OscLoaded() { return tables.loaded(); }
const Vc2OscTables& vc2OscTables() { return tables.t; }
