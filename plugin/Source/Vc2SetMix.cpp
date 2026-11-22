// The VC-2 part's conversion sets' mix: see Vc2SetMix.h (v284).  The block writer's reads and writes in rom.4's order;
// the floats through XtFloat, as the SH-4 rounds.
#include "Vc2SetMix.h"
#include "Vc2SetEnvelopeTables.h"
#include "XtFloat.h"

namespace {
constexpr int32_t kCrossfadeAt = 128;   // [FW] the crossfade's table 0x89542034 in the level run from 0x89541f34
constexpr int32_t kCrossTop = 127;      // [FW] a pair's first gain by 127 less its byte
constexpr float kOne = 1.0f;            // fldi1

const Vc2SetEnvelopeTables& mixTables() { return vc2SetEnvelopeTables(); }

float runGain(int32_t at, float scale) { return vc2LevelRun(at, scale); }

// Off, or a type past the four: 0 at +0, `gain` at the four gains
void evenGains(uint32_t* block, float gain) {
    block[0] = 0;
    for (int k = 1; k <= 4; ++k) block[k] = floatWord(gain);
}

// Types 1 and 2 (0x8939bbe0, inline): the type, the level by its byte, 1.0 at +0xc and +4
void levelGains(const Vc2SetMixWords& w, uint32_t* block, float scale) {
    block[0] = w.type;
    block[2] = floatWord(runGain(w.level, scale));
    block[3] = floatWord(kOne);
    block[1] = floatWord(kOne);
}

void typeOne(const Vc2SetMixWords& w, uint32_t* block) { levelGains(w, block, mixTables().mixScale); }

void typeTwo(const Vc2SetMixWords& w, uint32_t* block) { levelGains(w, block, mixTables().mixScaleTwo); }

// Type 3, 0x8939bc40: each pair by 127 less its byte and by its byte (a byte past 127 reads the run either side)
void typeThree(const Vc2SetMixWords& w, uint32_t* block) {
    const float scale = mixTables().mixScale;
    block[0] = 0;
    block[1] = floatWord(runGain(kCrossfadeAt + kCrossTop - w.first, scale));
    block[2] = floatWord(runGain(kCrossfadeAt + w.first, scale));
    block[3] = floatWord(runGain(kCrossfadeAt + kCrossTop - w.second, scale));
    block[4] = floatWord(runGain(kCrossfadeAt + w.second, scale));
}

// Type 4 (inline): the type and 1.0 at +4
void typeFour(const Vc2SetMixWords& w, uint32_t* block) {
    block[0] = w.type;
    block[1] = floatWord(kOne);
}

void otherType(const Vc2SetMixWords&, uint32_t* block) { evenGains(block, mixTables().mixOther); }

// [FW] the writer's switch on the type less 1, unsigned (its table 0x8939bd08): types 1…4, else the other gains
using MixWay = void (*)(const Vc2SetMixWords&, uint32_t*);
constexpr std::array<MixWay, 4> kWays { typeOne, typeTwo, typeThree, typeFour };
} // namespace

void Vc2SetMixes::blockOf(const Vc2SetMixWords& w, uint32_t* block, Vc2SetReads&) {
    if (w.on == 0) {
        evenGains(block, mixTables().mixOff);
        return;
    }
    const uint32_t way = w.type - 1u;
    (way < kWays.size() ? kWays[way] : otherType)(w, block);
}

void Vc2SetMixes::construct(const Vc2SetChildCall& c, uint32_t* block, Vc2SetReads& reads) {
    blockOf(madeByKind(c, reads), block, reads);
}
