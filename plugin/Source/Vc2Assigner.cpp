// The VC-2 part's assigner's first part: see Vc2Assigner.h (v277).  Each routine's reads, calls and writes in rom.4's
// order.
#include "Vc2Assigner.h"
#include "XtFloat.h"

namespace {
using V = Vc2PartVoices;
constexpr uint16_t kUpdateAsked = 1;          // [FW] the +0x30 bit a partner must not have
constexpr uint32_t kMarked = 0x20000000;      // [FW] the age word's bit a partner is taken at once by
constexpr int8_t kOneOfKind = 1;              // [FW] the kind mono's two retunes part by
constexpr uint32_t kTruncated = 0xbfffffff;   // [FW] the orders' bit 30, which a truncation clears
constexpr uint32_t kOrderAt = 0x20, kOrderBytes = 4;
constexpr uint8_t kTruncatedState = 1;        // [FW] a voice's state when its age is its own
constexpr uint8_t kClassByte52 = 0xff;        // [FW] the class builder's +0x52
} // namespace

bool Vc2Assigner::sameNote(uint32_t voice, const Vc2AssignCall& call, Vc2PartVoices& v, Vc2AssignParts& parts) {
    const Vc2VoiceNode& n = v.at(voice);
    return n.channel == parts.tableOf(call.message) && n.part == call.part && (n.updated & kUpdateAsked) == 0
           && n.state == 0 && (uint8_t)parts.kindOf(n.note) == call.kind;
}

Vc2Partner Vc2Assigner::partner(const Vc2AssignCall& call, Vc2PartVoices& v, Vc2AssignParts& parts) {
    Vc2Partner found;
    const uint32_t sounding = v.g.lists[V::kSounding];
    for (uint32_t voice = v.at(sounding).next; voice != sounding; voice = v.at(voice).next) {
        if (!sameNote(voice, call, v, parts)) continue;
        found.voice = voice;
        found.marked = (v.ageOf(voice) & kMarked) != 0;
        if (found.marked) break;
    }
    return found;
}

bool Vc2Assigner::matching(const Vc2AssignCall& call, Vc2PartVoices& v, Vc2AssignParts& parts) {
    const uint32_t sounding = v.g.lists[V::kSounding];
    for (uint32_t voice = v.at(sounding).next; voice != sounding; voice = v.at(voice).next)
        if (sameNote(voice, call, v, parts)) return true;
    return false;
}

void Vc2Assigner::splice(Vc2PartVoices& v) {
    const uint32_t sounding = v.g.lists[V::kSounding], prepared = v.g.lists[V::kPrepared];
    const uint32_t first = v.at(prepared).next;
    if (first == prepared) return;
    const uint32_t head = v.at(sounding).next, last = v.at(prepared).prev;
    v.at(last).next = head;
    v.at(first).prev = sounding;
    v.at(sounding).next = first;
    v.at(head).prev = last;
    v.at(prepared).next = prepared;
    v.at(prepared).prev = prepared;
}

void Vc2Assigner::start(const Vc2AssignCall& call, Vc2PartVoices& v, Vc2AssignParts& parts) {
    const uint32_t prepared = v.g.lists[V::kPrepared];
    for (uint32_t voice = v.at(prepared).next; voice != prepared; voice = v.at(voice).next) {
        const Vc2Partner p = partner(call, v, parts);
        Vc2Legato legato { (uint8_t)(p.voice != 0), (uint8_t)p.marked, (uint8_t)call.onByte };
        if (p.voice != 0) legato.onByte = (uint8_t)parts.onByteOf(v.at(p.voice).note);
        parts.prepare(voice, call, legato);
        const uint32_t partnerNote = p.voice != 0 ? v.at(p.voice).note : 0;
        parts.noteOn(v.at(voice).note, partnerNote, (int8_t)legato.onByte);
        const float mine = v.cost(voice, parts);
        float total = xtf::add(v.costOf(v.g.lists[V::kSounding], parts), mine);
        while (!(v.g.budget > total)) {
            const uint32_t stolen = v.oldest(parts);
            if (stolen == 0) break;
            total = xtf::sub(total, v.cost(stolen, parts));
            v.toFree(stolen, parts);
        }
        parts.startNote(v.at(voice).note);
    }
    splice(v);
}

void Vc2Assigner::build(const Vc2AssignCall& call, Vc2PartVoices& v, Vc2AssignParts& parts) {
    const uint32_t voice = v.take(parts);
    if (voice != 0) {
        Vc2Voice& w = v.voice(voice);
        vc2ConstructNode(w.node, voice);
        w.node.vtable = kNoteVtable;
        w.byte50 = 0;
        w.byte52 = kClassByte52;
        w.node.leave = V::kDestroyOnLeave;
    }
    if (v.preparedAny()) start(call, v, parts);
}

void Vc2Assigner::retune(const Vc2AssignCall& call, uint32_t value, bool kindOne, Vc2PartVoices& v,
                         Vc2AssignParts& parts) {
    const uint32_t sounding = v.g.lists[V::kSounding];
    for (uint32_t voice = v.at(sounding).next; voice != sounding; voice = v.at(voice).next) {
        const Vc2VoiceNode& n = v.at(voice);
        if (n.channel != parts.tableOf(call.message) || n.part != call.part || n.state != 0) continue;
        if ((parts.kindOf(n.note) == kOneOfKind) != kindOne) continue;
        parts.retune(voice, call.message, call.part, value);
    }
}

void Vc2Assigner::truncateSame(const Vc2AssignCall& call, Vc2PartVoices& v, Vc2AssignParts& parts) {
    const uint32_t sounding = v.g.lists[V::kSounding];
    for (uint32_t voice = v.at(sounding).next; voice != sounding; voice = v.at(voice).next) {
        const Vc2VoiceNode& n = v.at(voice);
        if (n.channel != parts.tableOf(call.message) || n.part != call.part) continue;
        if ((uint8_t)parts.kindOf(n.note) != call.kind) continue;
        truncateVoice(voice, v, parts);
        parts.waitQueue(kQueuesAt + kQueueBytes * n.queue, voice);
    }
}

void Vc2Assigner::truncateVoice(uint32_t voice, Vc2PartVoices& v, Vc2AssignParts& parts) {
    if (v.at(voice).note != 0) parts.truncateOnce(v.at(voice).note);
    clearOrders(voice, v);
}

void Vc2Assigner::clearOrders(uint32_t voice, Vc2PartVoices& v) {
    Vc2VoiceNode& n = v.at(voice);
    for (uint32_t& order : n.order) order &= kTruncated;
    if (n.age == voice + kOrderAt || n.age == voice + kOrderAt + kOrderBytes) n.state = kTruncatedState;
}

void Vc2Assigner::setState(uint32_t voice, uint32_t state, Vc2PartVoices& v) { v.at(voice).state = (uint8_t)state; }
