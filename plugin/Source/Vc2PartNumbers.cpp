// The VC-2 part's controller numbers and its init (v302): see Vc2PartNumbers.h.  The classes are rows (each one's
// vtable, its +0x14 handler, and whether its maker sets the Hold-release bytes); the init's switch on the number is rows
// of a number and its class, the base for any number not in them.  Each constructor writes the base's vtable
// (0x898cd220) and then its own; only its own stands.
#include "Vc2PartNumbers.h"
#include "Vc2PartSettingsTables.h"
#include <algorithm>
#include <iterator>
#include <utility>

namespace {
// [FW] a class: its vtable (its maker's), its handler (the vtable's +0x14), whether its maker sets each channel's
// Hold-release byte
struct NumberClass {
    uint32_t vtable, handler;
    bool holdLifts;
};
constexpr NumberClass kClasses[] = {
    { 0x898cd220, 0x893938d0, false },   // 0: the base (0x89393910), 1 and every number not below
    { 0x898cd250, 0x89393920, false },   // 1: 0x89393930
    { 0x898cd268, 0x89393950, false },   // 2: 0x89393960
    { 0x898cd238, 0x89393980, true },    // 3: Hold, 0x893939e0
    { 0x898cd280, 0x89393a10, false },   // 4: 0x89393a60
    { 0x898cd298, 0x89393a70, false },   // 5: 0x89393ab0
    { 0x898cd2b0, 0x89393ac0, false },   // 6: 0x89393b40
    { 0x898cd2c8, 0x89393ba0, false },   // 7: 0x89393c00
    { 0x898cd2e0, 0x89393c10, false },   // 8: 0x89393c70
    { 0x898cd2f8, 0x89393c80, false },   // 9: 0x89393cd0
    { 0x898cd310, 0x89393ce0, false },   // 10: 0x89393d80
    { 0x898cd328, 0x89393d90, false },   // 11: 0x89393da0 (its third argument, 1 for 6 and 0 for 38, unread)
};
// [FW] the init's switch: a number and its class
constexpr std::pair<uint8_t, uint8_t> kNumberClasses[] = {
    { 5, 2 },    { 65, 2 },  { 68, 2 },   { 71, 2 },   { 72, 2 },   { 73, 2 },   { 74, 2 },  { 7, 1 },
    { 10, 1 },   { 11, 1 },  { 91, 1 },   { 93, 1 },   { 64, 3 },   { 66, 4 },   { 84, 5 },  { 121, 6 },
    { 120, 7 },  { 126, 8 }, { 127, 9 },  { 123, 10 }, { 124, 10 }, { 125, 10 }, { 38, 11 }, { 6, 11 },
};
constexpr uint32_t kObjectBytes = sizeof(Vc2NumberObject);
// [FW] part object 0 (rom.4's table 0x89535458's first word), its plain class (the base's 0x898cd1fc written first)
constexpr uint32_t kPlainClass = 0x898cd184;
constexpr int8_t kNoChannel = -1;

const NumberClass& classOf(int number) {
    const auto* row = std::ranges::find_if(kNumberClasses, [number](const auto& r) { return r.first == number; });
    return kClasses[row == std::end(kNumberClasses) ? 0 : row->second];
}
} // namespace

void Vc2PartNumbers::make(Vc2PartRows& t) {
    for (int n = 0; n < kNumbers; ++n) {
        const NumberClass& c = classOf(n);
        Vc2NumberObject& o = objects[static_cast<size_t>(n)];
        o.vtable = c.vtable;
        o.number = static_cast<uint8_t>(n);
        if (c.holdLifts) t.holdLifts.fill(1);
        table[static_cast<size_t>(n)] = kObjectsAt + kObjectBytes * static_cast<uint32_t>(n);
    }
}

void Vc2PartNumbers::changed(int32_t number, int source, int32_t channel, int32_t value, Vc2NumberParts& parts) const {
    const uint32_t at = table[static_cast<size_t>(number)];
    const uint32_t vtable = objects[(at - kObjectsAt) / kObjectBytes].vtable;
    const auto* c = std::ranges::find_if(kClasses, [vtable](const NumberClass& k) { return k.vtable == vtable; });
    parts.handle(c->handler, at, source, channel, value);
}

void vc2PartInit(Vc2PartInitWorld& w) {
    Vc2PartObject& object = w.nodes.objects[0];
    for (int s = 0; s < Vc2PartNodes::kSources; ++s) {
        object.vtable = kPlainClass;
        Vc2PartNode& n = w.nodes.nodes[static_cast<size_t>(Vc2PartNodes::kParts * s)];
        n.object = Vc2PartNodes::kObjectsAt;
        w.keys.resetObjectGlides(object.index);
        n.byteE = 0;
        n.source = static_cast<int8_t>(s);
        n.channel = kNoChannel;
        w.t.heldNote = 0;
        n.word8 = vc2PartSettingsTables().nodeWord;
        object.index = 0;
    }
    w.keys.staticInit();
    w.keys.glidesInit();
    w.nodes.lists.fill(0);
    w.numbers.make(w.t);
}
