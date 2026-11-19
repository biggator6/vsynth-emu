// The VC-2 part's voices and their lists: see Vc2PartVoices.h (v272).  Every routine is rom.4's, read from its
// disassembly; a link is an address, resolved by at().  Notes' task.
#include "Vc2PartVoices.h"
#include "Vc2PartVoicesTables.h"
#include "XtFloat.h"
#include <algorithm>

namespace {
constexpr uint32_t kOrderAt = 0x20, kOrderBytes = 4;   // [FW] a voice's +0x20, which a prepare points its age at
constexpr uint32_t kPreparedOrder = 0xffffffff;        // [FW] the age a prepare gives it
constexpr int8_t kStealable = 0, kOneOfKind = 1;       // [FW] a note's kinds: the take steals 0; a cost counts one 1
constexpr uint32_t kLeastAge = 0xffffffff;             // [FW] the take's first least age

// The update queue's node's constructor 0x89394f30 on a node given: its links to itself
void constructNode(Vc2VoiceNode& n, uint32_t self) {
    n.vtable = Vc2PartVoices::kNodeVtable;
    n.queuePrev = self;
    n.queueNext = self;
}
} // namespace

Vc2VoiceNode& Vc2PartVoices::at(uint32_t address) {
    const auto& table = vc2PartVoicesTables().voices;
    const auto* v = std::ranges::find(table, address);
    if (v != table.end()) return voices[(size_t)(v - table.begin())].node;
    return heads[(address - kHeadsAt) / kHeadBytes % kLists];   // a head's, as any address not a voice's is taken
}

uint32_t Vc2PartVoices::addressOf(int voice) const { return vc2PartVoicesTables().voices[(size_t)voice]; }

void Vc2PartVoices::staticInit() {
    const auto& t = vc2PartVoicesTables();
    g.unread1 = t.unread[1];
    g.unread2 = t.unread[2];
    g.unread0 = t.unread[0];
    for (uint32_t k = 0; k < kLists; ++k) {
        constructNode(heads[k], kHeadsAt + kHeadBytes * k);
        heads[k].vtable = kVoiceVtable;
    }
}

void Vc2PartVoices::initVoices() {
    for (int n = 0; n < kVoices; ++n) {
        const uint32_t self = addressOf(n);
        Vc2VoiceNode& v = at(self);
        constructNode(v, self);
        v.vtable = kVoiceVtable;
        v.number = (uint8_t)n;
        v.age = kZeroAge;
        v.leave = kStayOnLeave;
        v.after = addressOf((n + 1) % kVoices);
        v.note = 0;
        v.state = kFreeState;
    }
}

void Vc2PartVoices::initLists() {
    for (uint32_t k = 0; k < kLists; ++k) g.lists[k] = kHeadsAt + kHeadBytes * k;
    for (const uint32_t head : g.lists) {
        at(head).next = head;
        at(head).prev = head;
    }
    for (int n = 0; n < kVoices; ++n) append(addressOf(n), g.lists[kFree]);
    g.reserved = 0;
    g.budget = vc2PartVoicesTables().budget;
    g.offset = 0.0f;
}

// At the tail of `list`, its links written in rom.4's order.
// one-off: rom.4's links are addresses resolved by at(), onto a head that is itself a node, in rom.4's order of
// writes; XtArpeggio's key ring links indices with no head and XtSoloEncoder's nodes pointers with none
void Vc2PartVoices::append(uint32_t voice, uint32_t list) {
    Vc2VoiceNode& v = at(voice);
    Vc2VoiceNode& l = at(list);
    const uint32_t last = l.prev;
    v.prev = last;
    v.next = list;
    l.prev = voice;
    at(last).next = voice;
}

void Vc2PartVoices::leave(uint32_t voice, Vc2VoiceParts& parts) {
    if (at(voice).leave == kDestroyOnLeave) parts.destroy(voice);
}

// A move's own part (each of the three's): out of its list, onto `list`'s tail with its state, then what +0x44 runs
void Vc2PartVoices::move(uint32_t voice, int list, uint8_t state, Vc2VoiceParts& parts) {
    Vc2VoiceNode& v = at(voice);
    const uint32_t before = v.prev, after = v.next;
    at(before).next = after;
    at(after).prev = before;
    append(voice, g.lists[(size_t)list]);
    v.state = state;
    leave(voice, parts);
    v.leave = kStayOnLeave;
}

void Vc2PartVoices::toPrepared(uint32_t voice, uint8_t state, Vc2VoiceParts& parts) {
    Vc2VoiceNode& v = at(voice);
    v.order[0] = kPreparedOrder;
    v.age = voice + kOrderAt;
    move(voice, kPrepared, state, parts);
}

// Freed or reserved: its age the zero's, and the next note's age 0 again when no voice sounds after it
void Vc2PartVoices::stopped(uint32_t voice, int list, uint8_t state, Vc2VoiceParts& parts) {
    at(voice).age = kZeroAge;
    move(voice, list, state, parts);
    const uint32_t sounding = g.lists[kSounding];
    if (at(sounding).next == sounding) ages = 0;
}

void Vc2PartVoices::toFree(uint32_t voice, Vc2VoiceParts& parts) { stopped(voice, kFree, kFreeState, parts); }
void Vc2PartVoices::toReserved(uint32_t voice, Vc2VoiceParts& parts) {
    stopped(voice, kReserved, kReservedState, parts);
}

// Its age: the zero's word, else one of its own two, which only a prepare points it at (+0x20)
uint32_t Vc2PartVoices::ageOf(uint32_t voice) {
    const Vc2VoiceNode& v = at(voice);
    if (v.age == kZeroAge) return vc2PartVoicesTables().zeroAge;
    return v.order[v.age == voice + kOrderAt + kOrderBytes ? 1 : 0];
}

uint32_t Vc2PartVoices::oldest(Vc2VoiceParts& parts) {
    const uint32_t list = g.lists[kSounding];
    uint32_t best = 0, least = kLeastAge;
    for (uint32_t v = at(list).next; v != list; v = at(v).next) {
        if (parts.kindOf(at(v).note) != kStealable) continue;
        const uint32_t age = ageOf(v);
        if (least <= age) continue;
        least = age;
        best = v;
    }
    return best;
}

uint32_t Vc2PartVoices::take(Vc2VoiceParts& parts) {
    const uint32_t free = g.lists[kFree], first = at(free).next;
    const uint32_t voice = first != free ? first : oldest(parts);
    if (voice) toPrepared(voice, 0, parts);
    return voice;
}

uint32_t Vc2PartVoices::stop(uint32_t voice, Vc2VoiceParts& parts) {
    if (voice) toFree(voice, parts);
    return voice;
}

void Vc2PartVoices::reserve(int32_t count, int32_t offset, Vc2VoiceParts& parts) {
    g.reserved = count;
    const auto top = (int32_t)((uint32_t)count + (uint32_t)offset);
    int32_t i = kVoices - 1;
    for (; i >= top; --i)
        if (at(addressOf(i)).state != kReservedState) toReserved(addressOf(i), parts);
    for (; i >= offset; --i)
        if (at(addressOf(i)).state > kFreeState) toFree(addressOf(i), parts);
    for (; i >= 0; --i)
        if (at(addressOf(i)).state != kReservedState) toReserved(addressOf(i), parts);
}

void Vc2PartVoices::endedScan(Vc2VoiceParts& parts) {
    const uint32_t list = g.lists[kSounding];
    uint32_t v = at(list).next;
    while (v != list) {
        if (!parts.ended(parts.toneOf(at(v).note))) {
            v = at(v).next;
            continue;
        }
        toFree(v, parts);
        v = at(list).next;   // from the list's start again
    }
}

void Vc2PartVoices::stopAll(Vc2VoiceParts& parts) {
    const uint32_t list = g.lists[kSounding];
    for (uint32_t v = at(list).next; v != list; v = at(list).next) toFree(v, parts);
}

float Vc2PartVoices::cost(uint32_t voice, Vc2VoiceParts& parts) {
    const float tone = parts.cost(parts.toneOf(at(voice).note));
    return xtf::add(tone, g.offset);
}

float Vc2PartVoices::costOf(uint32_t list, Vc2VoiceParts& parts) {
    float sum = 0.0f;
    bool counted = false;   // a voice of kind 1 counted: no other of that kind is
    for (uint32_t v = at(list).next; v != list; v = at(v).next) {
        const bool oneOfKind = parts.kindOf(at(v).note) == kOneOfKind;
        if (oneOfKind && counted) continue;
        counted = counted || oneOfKind;
        sum = xtf::add(sum, cost(v, parts));
    }
    return sum;
}

Vc2VoiceCount Vc2PartVoices::sounding(Vc2VoiceParts& parts) {
    Vc2VoiceCount c;
    const uint32_t list = g.lists[kSounding];
    for (uint32_t v = at(list).next; v != list; v = at(v).next) {
        ++c.count;
        c.sum = xtf::add(c.sum, cost(v, parts));
    }
    return c;
}

void Vc2PartVoices::setBudget(int32_t power) {
    g.budget = xtf::div(xtf::flt(power), vc2PartVoicesTables().perBudget);
}

void Vc2PartVoices::setOffset(int32_t power) {
    g.offset = xtf::div(xtf::flt(power), vc2PartVoicesTables().perOffset);
}

Vc2VoiceCount Vc2PartVoices::budget() const { return { g.reserved, g.budget }; }

bool Vc2PartVoices::preparedAny() {
    const uint32_t list = g.lists[kPrepared];
    return at(list).next != list;
}
