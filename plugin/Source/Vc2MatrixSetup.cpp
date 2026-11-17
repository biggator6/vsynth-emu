// rom.4's Matrix set-up: see Vc2MatrixSetup.h (v247).  Every address is rom.4's.
#include "Vc2MatrixSetup.h"
#include "Vc2MatrixSources.h"
#include "XtFloat.h"
#include <algorithm>
#include <cstring>
#include <type_traits>

namespace {
// [FW] what a record reads: Patch Category, Pitch Bend Range Up and Down, then from Matrix Control 1's Source five a
// control (Source, Dest 1, Sens 1, Dest 2, Sens 2)
constexpr int16_t kCategory = 0x12c, kBendUp = 0x13b, kBendDown = 0x13c, kControls = 0x16a, kControlStep = 5;
constexpr uint32_t kBendScale = 1000;   // [FW] a bend range × 1000, kept as int16
// [FW] a source held to the choice's: 32 none, past its last case (109) none
constexpr int32_t kNoSource = 32, kLastCase = 109;
// [FW] Patch Category 10 and 11 take velocity and keyfollow off Matrix Control 2
constexpr int32_t kCategoryA = 10, kCategoryB = 11, kVelocity = 107, kKeyfollow = 108;
constexpr int kControl2 = 1;
// [FW] the system's assignments: Beam 2 and 3 L, R (0 and 32 as 0xff), then the sources' seven
constexpr std::array<int16_t, 4> kBeams { 0x8b, 0x8c, 0x8d, 0x8e };
constexpr std::array<int16_t, 7> kAssigns { 0x8f, 0x90, 0x91, 0x92, 0x95, 0x96, 0xba };
constexpr uint8_t kNoBeam = 0xff;
// [FW] the zeros a record's uses and M's init take: the tables' run from 0x895402f6, the curve's 128th word on
constexpr int kZeros = 128;
constexpr uint32_t kLastDest = 78;        // [FW] the destination routines' switch
constexpr std::size_t kLevelGroup = 2;    // [FW] the levels' reader, 0x893a24d0: destinations 9…11

// A control's Source parameter as a record keeps it
int32_t heldSource(int32_t v) { return v == kNoSource || v > kLastCase ? 0 : v; }

// Matrix Control 2's: under Patch Category 10 or 11 no velocity or keyfollow
int32_t control2Source(int32_t v, int32_t category) {
    const bool off = (category == kCategoryA || category == kCategoryB) && (v == kVelocity || v == kKeyfollow);
    return off ? 0 : heldSource(v);
}

// The run's words from the zeros' start on, into M's words in order
template <class Words>
void fromZeros(Words& words, int& at) {
    for (auto& w : words) w = static_cast<std::remove_reference_t<decltype(w)>>(vc2MatrixCurveWord(at++));
}
} // namespace

int32_t vc2MatrixDestination(int32_t source, int32_t dest, int32_t sens, uint16_t* uses) {
    if (source == 0 || sens == 0 || static_cast<uint32_t>(dest) > kLastDest) return 0;
    const Vc2MatrixDest d = vc2MatrixDestOf(dest);
    uses[d.group] = static_cast<uint16_t>(uses[d.group] | d.bit);
    return d.dest;
}

int16_t vc2MatrixDepth(uint8_t source, int32_t dest, int32_t sens) {
    if (source == 0 || static_cast<uint32_t>(dest) > kLastDest) return 0;
    // Past ±128 no store's sensitivity reaches (its 7 bits give −64…63); rom.4 would read on past its tables
    if (sens < -Vc2MatrixTables::kSensReach || sens >= Vc2MatrixTables::kSensReach) return 0;
    return vc2MatrixDepthOf(dest, sens);
}

bool Vc2MatrixSetup::record(Vc2MatrixSettings& g, int part, bool held) {
    if (held) return false;
    Vc2MatrixFront& rec = g.parts[part];
    const int32_t category = params_.partValue(part, kCategory);
    const auto up = static_cast<int16_t>(static_cast<uint32_t>(params_.partValue(part, kBendUp)) * kBendScale);
    const auto down = static_cast<int16_t>(static_cast<uint32_t>(params_.partValue(part, kBendDown)) * kBendScale);
    std::memcpy(rec.head, &up, sizeof up);
    std::memcpy(rec.head + sizeof up, &down, sizeof down);
    int at = kZeros;
    fromZeros(rec.uses, at);
    for (int k = 0; k < Vc2MatrixFront::kSources; ++k) {
        const auto id = static_cast<int16_t>(kControls + kControlStep * k);
        const int32_t v = params_.partValue(part, id);
        const int32_t source = k == kControl2 ? control2Source(v, category) : heldSource(v);
        rec.sources[k] = static_cast<uint8_t>(source);
        rec.routines[k] = vc2MatrixSourceRoutine(source);
        for (int j = 0; j < 2; ++j) {
            const int32_t dest = params_.partValue(part, static_cast<int16_t>(id + 1 + 2 * j));
            const int32_t sens = params_.partValue(part, static_cast<int16_t>(id + 2 + 2 * j));
            Vc2MatrixSlot& slot = rec.slots[2 * k + j];
            slot.dest = static_cast<uint8_t>(vc2MatrixDestination(source, dest, sens, rec.uses));
            slot.depth = vc2MatrixDepth(static_cast<uint8_t>(source), dest, sens);
        }
    }
    return true;
}

void Vc2MatrixSetup::assigns(Vc2MatrixSettings& g) {
    for (std::size_t i = 0; i < kBeams.size(); ++i) {
        const int32_t v = params_.systemValue(kBeams[i]);
        g.beams[i] = v == 0 || v == kNoSource ? kNoBeam : static_cast<uint8_t>(v);
    }
    for (std::size_t i = 0; i < kAssigns.size(); ++i)
        g.assigns[i] = static_cast<uint8_t>(params_.systemValue(kAssigns[i]));
}

bool Vc2MatrixSetup::settings(Vc2MatrixSettings& g, int part, bool held) {
    assigns(g);
    return record(g, part, held);
}

void Vc2MatrixSetup::init(Vc2MatrixWords& m, Vc2MatrixLinks& links, const Vc2MatrixSettings& g,
                          Vc2MatrixRefresh& refresh) {
    const Vc2MatrixFront& rec = g.parts[links.part];
    std::ranges::copy(rec.sources, m.sources);
    std::ranges::copy(rec.uses, m.uses);
    std::ranges::copy(rec.slots, m.slots);
    std::ranges::copy(rec.routines, m.routines);
    int at = kZeros;
    fromZeros(m.dirty, at);
    fromZeros(m.last, at);
    fromZeros(m.terms, at);
    fromZeros(m.sums, at);
    links.channel = g.channel;
    levels(m, refresh);
}

void Vc2MatrixSetup::levels(Vc2MatrixWords& m, Vc2MatrixRefresh& refresh) {
    const Vc2MatrixGroup& group = vc2MatrixGroups()[kLevelGroup];
    for (std::size_t i = 0; i < levelsOut.values.size(); ++i)
        levelsOut.values[i] = xtf::mul(xtf::flt(vc2MatrixRead(m, group, refresh)[i]), vc2MatrixLevelScale());
    part_.levelsMoved();
}

void vc2MatrixHead(Vc2MatrixWords& m, const Vc2MatrixLinks& links, int32_t part, const std::array<uint8_t, 3>& bytes) {
    if (links.part != part) return;
    std::ranges::copy(bytes, m.head);
}
