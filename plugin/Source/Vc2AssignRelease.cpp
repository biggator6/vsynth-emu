// The VC-2 part's assigner's second part, the release path: see Vc2Assigner.h (v278).  Each routine's reads, calls and
// writes in rom.4's order.
#include "Vc2Assigner.h"
#include "Vc2Keys.h"
#include "Vc2KeysTables.h"

namespace {
using V = Vc2PartVoices;
constexpr uint8_t kKeyHeld = 1, kHoldHeld = 2, kSostenutoHeld = 4;   // [FW] a voice's +0x48 bits
constexpr uint8_t kPedalsHeld = kHoldHeld | kSostenutoHeld, kAnyHeld = kKeyHeld | kPedalsHeld;
constexpr uint32_t kSustainCleared = 0xefffffff;   // [FW] the age word's bit 28, cleared when no pedal holds
constexpr uint32_t kKeyCleared = 0xdfffffff;       // [FW] and bit 29, cleared at the key's release
constexpr uint8_t kPedalDown = 64;                 // [FW]
constexpr int8_t kOneOfKind = 1;                   // [FW] the kind released apart
constexpr uint32_t kKindOneHeld = 0x18;            // [FW] part 0's settings byte that keeps a kind-1 voice
constexpr int32_t kKeysTo = 127;
constexpr uint16_t kUpdateAsked = 1;               // [FW] a voice's +0x30 bit 0 (v344)

// After a held bit is cleared: with no pedal holding, the age word's bit 28
void afterHeld(Vc2VoiceNode& n) {
    if ((n.held & kPedalsHeld) == 0) n.order[0] &= kSustainCleared;
}

// A pedal's lift on a voice: its bit cleared when set and the key is up, then the key-off when nothing holds
void lift(uint32_t voice, uint8_t bit, Vc2PartVoices& v, Vc2AssignParts& parts) {
    Vc2VoiceNode& n = v.at(voice);
    if ((n.held & bit) == 0 || (n.held & kKeyHeld) != 0) return;
    n.held &= (uint8_t)~bit;
    afterHeld(n);
    if ((n.held & kAnyHeld) == 0) parts.keyOff(voice);
}

// Each sounding voice that passes `test`, given to `each`, its next link read after (v305; eachOf's loop until then)
template <class Test, class Each>
void eachWhere(Vc2PartVoices& v, Test test, Each each) {
    const uint32_t sounding = v.g.lists[V::kSounding];
    for (uint32_t voice = v.at(sounding).next; voice != sounding; voice = v.at(voice).next)
        if (test(v.at(voice))) each(voice);
}

// Each sounding voice of the key table at `table`, given to `each`
template <class Each>
void eachOf(uint32_t table, Vc2PartVoices& v, Each each) {
    eachWhere(v, [table](const Vc2VoiceNode& n) { return n.channel == table; }, each);
}

// A pedal below 64: its lift on each voice of the table
void pedalOff(uint32_t table, uint8_t pedal, uint8_t bit, Vc2PartVoices& v, Vc2AssignParts& parts) {
    if (pedal >= kPedalDown) return;
    eachOf(table, v, [&](uint32_t voice) { lift(voice, bit, v, parts); });
}
} // namespace

void Vc2Assigner::release(uint32_t voice, const Vc2KeyTable& keys, Vc2PartVoices& v, Vc2AssignParts& parts) {
    Vc2VoiceNode& n = v.at(voice);
    if ((n.held & kKeyHeld) == 0) return;
    if (keys.hold < kPedalDown) n.held &= (uint8_t)~kHoldHeld;
    const uint32_t key = n.byte42;
    const bool latched = (keys.latched[key >> 4] & vc2KeysTables().bits[key & 15]) != 0;
    if (keys.sostenuto < kPedalDown || !latched) n.held &= (uint8_t)~kSostenutoHeld;
    afterHeld(n);
    n.held &= (uint8_t)~kKeyHeld;
    n.byte2f = keys.offVelocity;
    n.order[0] &= kKeyCleared;
    if ((n.held & kAnyHeld) == 0) parts.keyOff(voice);
}

void Vc2Assigner::liftHold(uint32_t voice, Vc2PartVoices& v, Vc2AssignParts& parts) {
    lift(voice, kHoldHeld, v, parts);
}

void Vc2Assigner::liftSostenuto(uint32_t voice, Vc2PartVoices& v, Vc2AssignParts& parts) {
    lift(voice, kSostenutoHeld, v, parts);
}

void Vc2Assigner::noteOff(uint32_t table, Vc2Keys& keys, Vc2PartVoices& v, Vc2AssignParts& parts) {
    const Vc2KeyTable& k = keys.table(table);
    uint32_t kindOne = 0;
    eachOf(table, v, [&](uint32_t voice) {
        const Vc2VoiceNode& n = v.at(voice);
        const uint32_t offKey = (uint8_t)k.offKey;
        if ((uint32_t)(int8_t)n.byte2e > (uint32_t)(int8_t)k.offs[offKey]) return;
        if (parts.kindOf(n.note) == kOneOfKind) {
            kindOne = voice;
            return;
        }
        if (n.byte42 == offKey) release(voice, k, v, parts);
    });
    if (kindOne == 0 || vc2KeysHighestIn(k.held, 0, kKeysTo) >= 0) return;
    if (parts.partSetting(0, kKindOneHeld) == 0) release(kindOne, k, v, parts);
}

void Vc2Assigner::holdOff(uint32_t table, Vc2Keys& keys, Vc2PartVoices& v, Vc2AssignParts& parts) {
    pedalOff(table, keys.table(table).hold, kHoldHeld, v, parts);
}

void Vc2Assigner::sostenutoOff(uint32_t table, Vc2Keys& keys, Vc2PartVoices& v, Vc2AssignParts& parts) {
    pedalOff(table, keys.table(table).sostenuto, kSostenutoHeld, v, parts);
}

void Vc2Assigner::truncateQueued(uint32_t voice, Vc2PartVoices& v, Vc2AssignParts& parts) {
    truncateVoice(voice, v, parts);
    parts.waitQueue(kQueuesAt + kQueueBytes * v.at(voice).queue, voice);
}

void Vc2Assigner::truncateTable(uint32_t table, Vc2PartVoices& v, Vc2AssignParts& parts) {
    eachOf(table, v, [&](uint32_t voice) { truncateQueued(voice, v, parts); });
}

// v344: released 0 and an update asked for, then the truncation and the wait
void Vc2Assigner::truncateAsked(uint32_t voice, Vc2PartVoices& v, Vc2AssignParts& parts) {
    Vc2VoiceNode& n = v.at(voice);
    n.released = 0;
    n.updated = (uint16_t)(n.updated | kUpdateAsked);
    truncateQueued(voice, v, parts);
}

void Vc2Assigner::truncatePart(uint32_t word, Vc2PartVoices& v, Vc2AssignParts& parts) {
    const auto ofWord = [word](const Vc2VoiceNode& n) { return n.part == word; };
    eachWhere(v, ofWord, [&](uint32_t voice) { truncateAsked(voice, v, parts); });
}

void Vc2Assigner::truncateAll(Vc2PartVoices& v, Vc2AssignParts& parts) {
    const auto any = [](const Vc2VoiceNode&) { return true; };
    eachWhere(v, any, [&](uint32_t voice) { truncateAsked(voice, v, parts); });
}

// [FW] the walk begun again from the list's first after each stop, as the free list's move takes the voice off it
void Vc2Assigner::stopPart(uint32_t word, Vc2PartVoices& v, Vc2AssignParts& parts) {
    const uint32_t sounding = v.g.lists[V::kSounding];
    bool stopped = false;
    uint32_t voice = v.at(sounding).next;
    while (voice != sounding) {
        if (v.at(voice).part != word || parts.changed(voice, word) == 0) {
            voice = v.at(voice).next;
            continue;
        }
        v.toFree(voice, parts);
        stopped = true;
        voice = v.at(sounding).next;
    }
    if (stopped) parts.allNotesOff();
}

void Vc2Assigner::releaseBut(uint32_t table, Vc2Keys& keys, Vc2PartVoices& v, Vc2AssignParts& parts) {
    eachOf(table, v, [&](uint32_t voice) {
        const bool kept = parts.kindOf(v.at(voice).note) == kOneOfKind && parts.partSetting(0, kKindOneHeld) != 0;
        if (!kept) release(voice, keys.table(table), v, parts);
    });
}

void Vc2Assigner::releaseAll(uint32_t table, Vc2Keys& keys, Vc2PartVoices& v, Vc2AssignParts& parts) {
    eachOf(table, v, [&](uint32_t voice) { release(voice, keys.table(table), v, parts); });
}

void Vc2Assigner::notify(uint32_t state, Vc2Keys& keys, Vc2PartVoices& v, Vc2AssignParts& parts) {
    const auto onState = [&](const Vc2VoiceNode& n) { return keys.table(n.channel).channel == state; };
    eachWhere(v, onState, [&](uint32_t voice) { parts.changed(voice, v.at(voice).part); });
}
