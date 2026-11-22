// The VC-2 part's settings by ID, the system's readers the parameters' messages run (v296): see Vc2PartSettings.h.
// Their reads, stores and calls out in rom.4's order; each answers 1, and none tests a held flag.
#include "Vc2PartSettings.h"
#include <algorithm>

namespace {
using Block = Vc2ParamBlock;
// [FW] Time Trip Assign X and Y (the system's), Patch Remain (its second block's); Number of Voices, Offset of Voice
// Number, CPU Power Max for TG and CPU Power Offset for Voice (its third's)
constexpr int16_t kTripIds[] = { 0x93, 0x94 };
constexpr int16_t kPatchRemain = 0x41;
constexpr int16_t kVoices = 0x52d, kVoiceOffset = 0x52e, kPowerMax = 0x530, kPowerOffset = 0x531;
// [FW] an assignment's two values for none, stored as −1; the voices' most and the offset's
constexpr int32_t kNoAssign = 0, kNoAssignToo = 32, kMostVoices = 24, kMostOffset = 23;
constexpr int8_t kNone = -1;
} // namespace

void Vc2PartSettings::tripAssigns(Vc2PartRows& t) {
    for (size_t k = 0; k < std::size(kTripIds); ++k) {
        const int32_t v = p_.value(Block::kSystem, 0, 0, kTripIds[k]);
        t.tripAssigns[k] = v == kNoAssign || v == kNoAssignToo ? kNone : static_cast<int8_t>(v);
    }
}

void Vc2PartSettings::patchRemain(Vc2PartRows& t) {
    t.patchRemain = static_cast<uint8_t>(p_.value(Block::kSystemB, 0, 0, kPatchRemain));
}

void Vc2PartSettings::voiceCounts(Vc2PartRows& t, Vc2SettingsParts& out) {
    const auto get = [&](int16_t id) { return p_.value(Block::kSystemC, 0, 0, id); };
    int32_t count = std::min(get(kVoices), kMostVoices);
    const int32_t offset = std::min(get(kVoiceOffset), kMostOffset);
    const int32_t power = get(kPowerMax);
    const int32_t powerOffset = get(kPowerOffset);
    const auto both = static_cast<int32_t>(static_cast<uint32_t>(count) + static_cast<uint32_t>(offset));
    if (both > kMostVoices) count = kMostVoices - offset;
    std::array<int16_t, 4>& last = t.voiceCounts;
    if (last[0] != count || last[1] != offset) {
        last[0] = static_cast<int16_t>(count);
        last[1] = static_cast<int16_t>(offset);
        out.reserve(count, offset);
    }
    if (last[2] != power) {
        last[2] = static_cast<int16_t>(power);
        budget(t, out);
    }
    if (last[3] != powerOffset) {
        last[3] = static_cast<int16_t>(powerOffset);
        out.setPowerOffset(powerOffset);
    }
}
