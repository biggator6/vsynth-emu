// The VC-2 part's controller numbers and its init (v302; the handlers v303): see Vc2PartNumbers.h.  The classes are
// rows (each one's vtable, its +0x14 handler's address and the port's way, and whether its maker sets the Hold-press
// bytes); the init's switch on the number is rows of a number and its class, the base for any number not in them.
// Each constructor writes the base's vtable (0x898cd220) and then its own; only its own stands.  A handler's key table
// is the source's (counting from 1) and channel's; the channel's state its row's word + 2 (rom.4's tables, v276's).
#include "Vc2PartNumbers.h"
#include "Vc2KeysTables.h"
#include "Vc2PartSettingsTables.h"
#include <algorithm>
#include <iterator>
#include <utility>

namespace {
// A handler's call: the source (1…4), the channel and the value
struct Hand {
    int source;
    int32_t channel, value;
};
using Way = void (*)(Vc2NumberWorld&, const Hand&);

constexpr int32_t kNoChannel = -1, kPressed = 64, kMonoMost = 16;   // [FW]
constexpr uint32_t kStateWord = 2;                                   // [FW] a channel's state: its row's word + 2
constexpr uint8_t kNoKey = 0xff;                                     // [FW] CC 84's key cleared
constexpr int kGlidesASource = 2 * Vc2Keys::kChannels, kGlidesAnObject = 2;

uint32_t tableOf(const Hand& h) {
    return Vc2Keys::tableAt(static_cast<uint32_t>(h.source - 1), static_cast<uint32_t>(h.channel));
}

uint32_t stateOf(const Hand& h) {
    const Vc2KeysTables& k = vc2KeysTables();
    const auto row = static_cast<size_t>(k.sources[static_cast<size_t>(h.source - 1)]);
    return k.states[Vc2Keys::kChannels * row + static_cast<size_t>(h.channel)] + kStateWord;
}

// 0x89393000: each node on the source's channel's list, its two glides' held flags cleared
void clearGlideHolds(Vc2NumberWorld& w, const Hand& h) {
    const auto list = static_cast<size_t>(Vc2PartNodes::kChannels * (h.source - 1) + h.channel);
    for (uint32_t at = w.nodes.lists[list]; at != 0; at = w.nodes.node(at).next) {
        const Vc2PartNode& n = w.nodes.node(at);
        for (int g = 0; g < kGlidesAnObject; ++g) {
            Vc2KeyGlide& p = w.keys.glides[static_cast<size_t>(kGlidesASource * n.source + 2 * n.byteE + g)];
            p.lastMonoHeld = 0;
            p.lastPolyHeld = 0;
            p.prevPolyHeld = 0;
            p.prevMonoHeld = 0;
        }
    }
}

// The key table cleared, the glides' held flags, then a routine of the release path on the table
void clearThen(Vc2NumberWorld& w, const Hand& h, void (Vc2NumberParts::*then)(uint32_t)) {
    const uint32_t k = tableOf(h);
    w.keys.table(k).clear();
    clearGlideHolds(w, h);
    (w.parts.*then)(k);
}

// 0x893938d0: the channel's state to its listeners, no channel nothing
void base(Vc2NumberWorld& w, const Hand& h) {
    if (h.channel == kNoChannel) return;
    w.parts.notify(stateOf(h));
}

// 0x89393980: a press heard by the channel's Hold-press byte, a lift always
void hold(Vc2NumberWorld& w, const Hand& h) {
    base(w, h);
    if (h.value >= kPressed && w.t.holdPresses[static_cast<size_t>(h.channel)] == 0) return;
    const uint32_t k = tableOf(h);
    w.keys.table(k).setHold(static_cast<uint32_t>(h.value));
    w.parts.holdOff(k);
    w.parts.vocoderHold(h.value);
}

// 0x89393a10
void sostenuto(Vc2NumberWorld& w, const Hand& h) {
    base(w, h);
    const uint32_t k = tableOf(h);
    w.keys.table(k).setSostenuto(h.value);
    w.parts.sostenutoOff(k);
}

// 0x89393a70: CC 84's key, the key table's +0x130
void portamentoKey(Vc2NumberWorld& w, const Hand& h) {
    base(w, h);
    w.keys.table(tableOf(h)).wasReset = static_cast<uint8_t>(h.value);
}

// 0x89393ac0: both pedals up, CC 84's key cleared, the release path's pedals, the listeners (no test of the channel),
// the vocoder's pedal up
void resetAll(Vc2NumberWorld& w, const Hand& h) {
    const uint32_t k = tableOf(h);
    Vc2KeyTable& t = w.keys.table(k);
    t.setHold(0);
    t.setSostenuto(0);
    t.wasReset = kNoKey;
    w.parts.holdOff(k);
    w.parts.sostenutoOff(k);
    w.parts.notify(stateOf(h));
    w.parts.vocoderHold(0);
}

// 0x89393ba0: All Sound Off, with no note held and a value of 0
void soundOff(Vc2NumberWorld& w, const Hand& h) {
    if (w.t.heldNote != 0 || h.value != 0) return;
    clearThen(w, h, &Vc2NumberParts::truncateTable);
}

// 0x89393c10: Mono, a value of 16 or less
void mono(Vc2NumberWorld& w, const Hand& h) {
    if (h.value > kMonoMost) return;
    clearThen(w, h, &Vc2NumberParts::releaseBut);
}

// 0x89393c80: Poly, a value of 0
void poly(Vc2NumberWorld& w, const Hand& h) {
    if (h.value != 0) return;
    clearThen(w, h, &Vc2NumberParts::releaseBut);
}

// 0x89393ce0: the notes off, 123…125: below 0 every note released; 0, with no note held, but the held
void notesOff(Vc2NumberWorld& w, const Hand& h) {
    if (h.value < 0) {
        clearThen(w, h, &Vc2NumberParts::releaseAll);
        return;
    }
    if (h.value != 0 || w.t.heldNote != 0) return;
    clearThen(w, h, &Vc2NumberParts::releaseBut);
}

// [FW] a class: its vtable (its maker's), its handler (the vtable's +0x14) and the port's way, whether its maker sets
// each channel's Hold-press byte
struct NumberClass {
    uint32_t vtable, handler;
    Way way;
    bool holdPresses;
};
constexpr NumberClass kClasses[] = {
    { 0x898cd220, 0x893938d0, base, false },            // 0: the base (0x89393910), 1 and every number not below
    { 0x898cd250, 0x89393920, base, false },            // 1: 0x89393930
    { 0x898cd268, 0x89393950, base, false },            // 2: 0x89393960
    { 0x898cd238, 0x89393980, hold, true },             // 3: Hold, 0x893939e0
    { 0x898cd280, 0x89393a10, sostenuto, false },       // 4: 0x89393a60
    { 0x898cd298, 0x89393a70, portamentoKey, false },   // 5: 0x89393ab0
    { 0x898cd2b0, 0x89393ac0, resetAll, false },        // 6: 0x89393b40
    { 0x898cd2c8, 0x89393ba0, soundOff, false },        // 7: 0x89393c00
    { 0x898cd2e0, 0x89393c10, mono, false },            // 8: 0x89393c70
    { 0x898cd2f8, 0x89393c80, poly, false },            // 9: 0x89393cd0
    { 0x898cd310, 0x89393ce0, notesOff, false },        // 10: 0x89393d80
    { 0x898cd328, 0x89393d90, base, false },            // 11: 0x89393da0 (its third argument, 1 for 6 and 0 for 38, unread)
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

const NumberClass& classOf(int number) {
    const auto* row = std::ranges::find_if(kNumberClasses, [number](const auto& r) { return r.first == number; });
    return kClasses[row == std::end(kNumberClasses) ? 0 : row->second];
}

const NumberClass& classAt(const Vc2PartNumbers& p, uint32_t object) {
    const uint32_t vtable = p.objects[(object - Vc2PartNumbers::kObjectsAt) / kObjectBytes].vtable;
    return *std::ranges::find_if(kClasses, [vtable](const NumberClass& k) { return k.vtable == vtable; });
}
} // namespace

void Vc2PartNumbers::make(Vc2PartRows& t) {
    for (int n = 0; n < kNumbers; ++n) {
        const NumberClass& c = classOf(n);
        Vc2NumberObject& o = objects[static_cast<size_t>(n)];
        o.vtable = c.vtable;
        o.number = static_cast<uint8_t>(n);
        if (c.holdPresses) t.holdPresses.fill(1);
        table[static_cast<size_t>(n)] = kObjectsAt + kObjectBytes * static_cast<uint32_t>(n);
    }
}

uint32_t Vc2PartNumbers::objectOf(int32_t number) const { return table[static_cast<size_t>(number)]; }

uint32_t Vc2PartNumbers::handlerOf(int32_t number) const { return classAt(*this, objectOf(number)).handler; }

void Vc2PartNumbers::changed(int32_t number, int source, int32_t channel, int32_t value, Vc2NumberWorld& w) const {
    classAt(*this, objectOf(number)).way(w, { source, channel, value });
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
