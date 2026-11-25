// rom.4's system store: see Vc2SystemStore.h (v347).  Every address is rom.4's.
#include "Vc2SystemStore.h"
#include "Vc2Params.h"
#include "Vc2RomTables.h"
#include "XtFiles.h"
#include "XtFwMachine.h"
#include <algorithm>
#include <iterator>
#include <memory>

namespace {
constexpr int kSystemBlock = 24;   // [FW] 0x89320790: the system's block, system B's base + 24
constexpr int kLastId = 0x7fff;
// [FW] v457: Clock Source, External In Type (system B's); the controller assigns, Beam 1 L to Knob 2 (the system's)
constexpr int kClockSource = 0x43, kExternalInType = 0x7a, kAssignsFirst = 0x89, kAssignsLast = 0x96;
// [FW] v458: the effect driver's, as traced: its system reader's (0x8935feb0) on system B's, Attenuation on C's
constexpr int kMasterLevel = 0x40, kOutputGain = 0x78, kAttenuation = 0x52b;
constexpr int kFxSystemB[] = { kMasterLevel, kClockSource, 0x67, 0x68, 0x6f, 0x70, 0x71, 0x72, kOutputGain,
                               kExternalInType, 0x7f };

// [FW] each area's one section, every ID at its base: system B's, the system's, C's and D's
constexpr XtPatchSectionDef kAreas[][1] = { { { 0, kLastId, Vc2System::kRecord, 0 } },
                                            { { 0, kLastId, Vc2System::kRecord + kSystemBlock, 0 } },
                                            { { 0, kLastId, Vc2System::kC, 0 } },
                                            { { 0, kLastId, Vc2System::kD, 0 } } };
constexpr int kNoCosmFirst = 1, kNoCosmLast = 0;   // no ID a COSM's
constexpr XtStoreLayout kLayouts[] = {
    { kAreas[0], kNoCosmFirst, kNoCosmLast, 0, vc2ParamDesc, vc2TypedDesc },
    { kAreas[1], kNoCosmFirst, kNoCosmLast, 0, vc2ParamDesc, vc2TypedDesc },
    { kAreas[2], kNoCosmFirst, kNoCosmLast, 0, vc2ParamDesc, vc2TypedDesc },
    { kAreas[3], kNoCosmFirst, kNoCosmLast, 0, vc2ParamDesc, vc2TypedDesc },
};

// [FW] the defaults' routines, by Vc2System::kAreas, each on its area where rom.4 keeps it: the record's (0x89319e10's),
// C's (the store object's construction 0x8931a640 runs it) and D's (0x89319a40's with no flag)
constexpr uint32_t kDefaults[] = { 0x89328d60, 0x89329190, 0x89328d20 };
static_assert(std::size(kDefaults) == std::size(Vc2System::kAreas));

struct Read {
    std::unique_ptr<Vc2System> system;
    bool read(Sh4& image) {
        image.mapRam(xtfw::kRam, xtfw::kRamSize);
        image.stepLimit = xtfw::kCallSteps;
        auto s = std::make_unique<Vc2System>();
        for (size_t k = 0; k < std::size(kDefaults); ++k) {
            const Vc2System::Area& a = Vc2System::kAreas[k];
            if (!image.call(kDefaults[k], { a.at }, xtfw::kStack)) return false;
            image.read(a.at, s->bytes.data() + a.from, (size_t)a.size);
        }
        system = std::move(s);
        return true;
    }
};
constinit Vc2RomTables<Read> tables;
} // namespace

int vc2SystemValue(const Vc2System& s, Vc2SystemArea area, int id) {
    return xtStoreValue(kLayouts[(size_t)area], s.bytes, id);
}

void vc2SystemPut(Vc2System& s, Vc2SystemArea area, int id, int value) {   // v457
    if (const XtParamDesc* d = vc2ParamDesc(id))
        xtStoreRawPut(kLayouts[(size_t)area], s.bytes, id, 0, 0, (uint32_t)(value + d->centre));
}

bool vc2SystemAreaOf(int id, Vc2SystemArea& area) {   // v457
    if (id == kClockSource || id == kExternalInType) area = Vc2SystemArea::kB;
    else if (id >= kAssignsFirst && id <= kAssignsLast) area = Vc2SystemArea::kSystem;
    else return (id == kMasterLevel || id == kOutputGain) && vc2FxSystemAreaOf(id, area);   // v458
    return true;
}

bool vc2FxSystemAreaOf(int id, Vc2SystemArea& area) {   // v458
    if (id == kAttenuation) area = Vc2SystemArea::kC;
    else if (std::ranges::find(kFxSystemB, id) != std::end(kFxSystemB)) area = Vc2SystemArea::kB;
    else return false;
    return true;
}

bool vc2SystemDefault(int id, int& value) {   // v457
    Vc2SystemArea area {};
    const Vc2System* system = vc2SystemDefaults();
    if (system == nullptr || !vc2SystemAreaOf(id, area)) return false;
    value = vc2SystemValue(*system, area, id);
    return true;
}

bool vc2LoadSystem(const char* path) { return tables.load(path); }

const Vc2System* vc2SystemDefaults() { return tables.loaded() ? tables.t.system.get() : nullptr; }
