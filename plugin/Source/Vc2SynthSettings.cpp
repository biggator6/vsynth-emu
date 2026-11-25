// The part's settings: see Vc2SynthSettings.h (v345).  Every address is rom.4's.
#include "Vc2SynthSettings.h"
#include "Vc2KeysTables.h"
#include "Vc2Params.h"
#include "Vc2PatchStore.h"
#include "Vc2SynthControls.h"
#include <algorithm>
#include <cstring>
#include <iterator>

namespace {
using Block = Vc2ParamBlock;

// [FW] rom.4's getter on a read object never set up: an ID's default less its centre (0x8932b340 → 0x89328780), a typed
// ID's typed default (the typed getter's 0x8932b530 → 0x89328d10)
int32_t defaultOf(int16_t id) {
    if (id >= kVc2FirstTyped) {
        const XtTypedDesc* t = vc2TypedDesc(id);
        return t != nullptr ? t->def : 0;
    }
    const XtParamDesc* d = vc2ParamDesc(id);
    return d != nullptr ? d->def - d->centre : 0;
}

// The blocks in the part's store: the part's (0x893207b0) and a kind's three (0x89320a50, 0x89320ad0, 0x89320b70)
bool inPatch(Block b) { return b == Block::kPart || b == Block::kKindA || b == Block::kKindB || b == Block::kKindC; }

// [FW] v347: the system's blocks, each its area of the system store: the system's (0x89320790), B's (0x89320760), C's
// (0x89320f60) and D's (0x893206f0)
struct SystemBlock {
    Block block;
    Vc2SystemArea area;
};
constexpr SystemBlock kSystemBlocks[] = { { Block::kSystem, Vc2SystemArea::kSystem },
                                          { Block::kSystemB, Vc2SystemArea::kB },
                                          { Block::kSystemC, Vc2SystemArea::kC },
                                          { Block::kSystemD, Vc2SystemArea::kD } };
} // namespace

// The system's blocks from the system store once given; a part past 0 has no store (the VC-2's part 0 alone); a kind's
// blocks are its zone's
int32_t Vc2SynthSettings::value(Block block, int part, int kind, int16_t id) {
    const auto* system = std::ranges::find(kSystemBlocks, block, &SystemBlock::block);
    if (system != std::end(kSystemBlocks) && s_.hasSystem) return vc2SystemValue(s_.system, system->area, id);
    if (s_.patch == nullptr || part != 0 || !inPatch(block)) return defaultOf(id);
    return xtStoreValue(vc2Store(), *s_.patch, id, block == Block::kPart ? 0 : kind, 0);
}

int32_t Vc2SynthSettings::cosmValue(int part, int kind, int cosm, int16_t id) {
    if (s_.patch == nullptr || part != 0) return defaultOf(id);
    if (id >= kVc2FirstTyped) return vc2StoreTyped(*s_.patch, id, kind, cosm);
    return xtStoreValue(vc2Store(), *s_.patch, id, kind, cosm);
}

// [FW] 0x893a2e50's levels at 0x89997290, the static M's +0x188 and the mixer's offsets the same three floats; then the
// mixer's targets
void Vc2SynthSettings::levelsMoved() {
    s_.staticM.levels = setup.levelsOut;
    std::memcpy(&s_.mixer.offsets, setup.levelsOut.values.data(), sizeof s_.mixer.offsets);
    s_.mixer.targets();
}

// [FW] 0x893b1be0: the vocoder's settings path, then its second argument at 0x8993eac8 (the mixer's second word)
void Vc2SynthSettings::vocoderSettings(Vc2VocoderSettings& v, int32_t stereo) {
    s_.vocoder.settings(v);
    s_.mixer.w.stereo = stereo;
}

void Vc2SynthSettings::allSoundOff(int source, int32_t channel) {
    Vc2NumberWorld w { s_.rows, s_.nodes, s_.keys, *s_.controls };
    vc2NumbersSoundOff(source, channel, w);
}

void Vc2SynthSettings::controller(int source, int32_t channel, int32_t number, int32_t value) {
    s_.controls->controller(source, channel, number, value);
}

bool Vc2SynthSettings::receive() {
    Vc2PartWorld w = world();
    return settings.receive(w);
}

bool Vc2SynthSettings::routing(bool on) {
    Vc2PartWorld w = world();
    return settings.routing(w, on);
}

// [FW] the states' table 0x8950d6e0, 64 bytes a source, by the source byte at 0x8950d6d9
uint32_t Vc2SynthSettings::receiving(int32_t channel) {
    const Vc2KeysTables& t = vc2KeysTables();
    return t.states[(size_t)(16 * t.sources[0] + channel)];
}

void Vc2SynthSettings::parameters(uint8_t kind, uint8_t byte1) {
    Vc2PartWorld w = world();
    settings.parameters(w, kind, byte1);
}

void Vc2SynthSettings::partParameters(uint8_t kind, uint8_t part) {
    Vc2PartWorld w = world();
    settings.partParameters(w, kind, part);
}
