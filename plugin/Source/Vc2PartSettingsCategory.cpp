// The VC-2 part's settings by ID, the Category and budget 0x89392c60 (v294): see Vc2PartSettings.h.  Its reads, the
// held note's word and its calls out in rom.4's order; no held flag is tested.
#include "Vc2PartSettings.h"
#include <algorithm>

namespace {
using Block = Vc2ParamBlock;
constexpr int16_t kCategory = 0x12c, kPower = 0x530;   // [FW] Patch Category (part 0's), CPU Power Max for TG
// [FW] rom.4's switch on the Category, 0…11, as its law: 400 µs off the budget for each but 6, the held note for 10
// and 11; past 11 neither
constexpr uint32_t kCategories = 12;
constexpr int32_t kFreeCategory = 6, kHeldCategories[] = { 10, 11 };
constexpr uint32_t kLess = 400;
// [FW] the controller sent: from source 3, All Notes Off (123) with no value, on the channel of part 0's node of
// source 4; the voices truncated are those of the given node's word; Auto Note's and the all-notes-off read part 0
constexpr int kControlSource = 3, kNodeSource = 4, kGivenSource = 1, kPartZero = 0;
constexpr int32_t kAllNotesOff = 123, kNoValue = -1;
} // namespace

void Vc2PartSettings::category(Vc2PartRows& t, const Vc2PartNodes& nodes, int part, Vc2SettingsParts& out) {
    const auto get = [&](int16_t id) { return p_.value(Block::kPart, kPartZero, 0, id); };
    get(kCategory);   // read, and stored in 0x899891d0 as Category ≥ 10, which the switch writes again
    const int32_t category = get(kCategory);
    uint32_t power = static_cast<uint32_t>(p_.value(Block::kSystemC, 0, 0, kPower));
    const bool known = static_cast<uint32_t>(category) < kCategories;
    const bool held = known && std::ranges::find(kHeldCategories, category) != std::end(kHeldCategories);
    t.heldNote = held ? 1 : 0;
    if (known && category != kFreeCategory) power -= kLess;
    out.setBudget(static_cast<int32_t>(power));
    out.controller(kControlSource, nodes.of(kNodeSource, kPartZero).channel, kAllNotesOff, kNoValue);
    out.truncateVoices(nodes.of(kGivenSource, part).word8);
    autoNoteSwitch(out);
    if (t.heldNote == 0) out.allNotesOff();
}
