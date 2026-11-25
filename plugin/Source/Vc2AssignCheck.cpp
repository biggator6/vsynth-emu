// xt_vc2voices' second part (v277): the assigner's first part, Vc2Assigner, against rom.4's own routines on its own
// voices, heads and words, after the voices' cases on a generator of its own (the seed's next), so their cases stay as
// they were.  Beyond the voices' hooks (the destroys, the tone's end test and cost: Vc2PartVoicesFakes.cpp) the voice's
// +0x3c and +0x18 virtuals of both classes, the note's note-on 0x893965e0, start 0x89396630 and truncation once
// 0x89396910, and the update queue's wait 0x893951b0 are hooked to log their arguments (a +0x3c's stack struct packed,
// with the two stack words after it) into the same log; a +0x3c writes the voice's note as drawn, as rom.4's makes one.
// The messages (four portamento keys) aim at one of three key tables in rom.4's memory, the voices' key tables are
// those three or another, their parts 0…2, their update bit and queue drawn, and the notes' bytes.  Each case starts
// from noise and rom.4's three inits, then 400 events (Vc2AssignEvents.cpp); before each, the voices' draws and these;
// after each, the span word for word, the calls and the answer.  XT_VC2VOICES_MAIN only.
#ifdef XT_VC2VOICES_MAIN
#include "Vc2AssignCheck.h"
#include "Vc2CheckCalls.h"
#include <cstring>

namespace {
using V = Vc2PartVoices;
constexpr uint32_t kNoteAt = 0x8998a51c, kNoteBytes = 0x744;   // [FW] the notes W
constexpr uint32_t kRoutinesAt[4] = { 0x893965e0, 0x89396630, 0x89396910, 0x893951b0 };   // [FW] what's logged
constexpr VoicesKind kRoutineKinds[4] = { kVoicesNoteOn, kVoicesStartNote, kVoicesTruncateOnce, kVoicesWait };
constexpr int kRoutineRegs[4] = { 2, 0, 0, 1 };   // [FW] the registers past r4 each reads

uint32_t tableAt(int k) { return kAssignTablesAt + kAssignTableBytes * (uint32_t)k; }
uint32_t messageAt(int k) { return kAssignMessagesAt + kAssignMessageBytes * (uint32_t)k; }

uint32_t packLegato(const Vc2Legato& l) { return l.found | (uint32_t)l.marked << 8 | (uint32_t)l.onByte << 16; }

VoicesCall called(VoicesKind kind, uint32_t routine, uint32_t object, std::array<uint32_t, 7> more) {
    return { kind, routine, object, more };
}
} // namespace

void assignHookSlot(VoicesSides& s, VoicesKind kind, uint32_t entry, int registers, uint32_t (*answer)(VoicesSides&),
                    std::initializer_list<uint32_t> classes) {
    const std::initializer_list<uint32_t> both = { V::kVoiceVtable, kVoicesNoteClass };
    for (const uint32_t vtable : classes.size() != 0 ? classes : both) {
        const VoicesCall slot = voicesSlotCall(s.fw, kind, 0, vtable, entry);
        s.fw.cpu.hook(slot.routine, [&s, kind, registers, slot, answer](Sh4& c) {
            std::array<uint32_t, 7> more {};
            for (int n = 0; n < registers; ++n) more[(size_t)n] = c.r[5 + n];
            s.rom.push_back(called(kind, slot.routine, c.r[4], more));
            c.ret(answer != nullptr ? answer(s) : 0);
        });
    }
}

void AssignPortParts::slotCalled(VoicesKind kind, uint32_t voice, uint32_t entry, std::array<uint32_t, 7> more) {
    const VoicesCall slot = voicesSlotCall(s_.fw, kind, voice, s_.port.at(voice).vtable, entry);
    voicesLog(s_.mine, called(kind, slot.routine, slot.object, more));
}

// rom.4's side: the voice's two virtuals of both classes, the note's three routines and the queue's wait
void assignInstallRom(AssignSides& a) {
    VoicesSides& s = a.s;
    for (const uint32_t vtable : { V::kVoiceVtable, kVoicesNoteClass }) {
        const VoicesCall prepare = voicesSlotCall(s.fw, kVoicesPrepare, 0, vtable, kAssignPrepareEntry);
        s.fw.cpu.hook(prepare.routine, [&s, prepare](Sh4& c) {
            const uint32_t sp = c.r[15];
            const uint32_t legato = c.rd32(c.rd32(sp)) & 0xffffff;
            s.rom.push_back(called(kVoicesPrepare, prepare.routine, c.r[4],
                                   { c.r[5], c.r[6], c.r[7], legato, c.rd32(sp + 4), c.rd32(sp + 8), 0 }));
            c.wr32(c.r[4] - prepare.object + 0x4c, assignNoteOf(s.draws.next(s.draws.rom)));
            c.ret(0);
        });
    }
    assignHookSlot(s, kVoicesRetune, kAssignRetuneEntry, 3);
    for (size_t k = 0; k < std::size(kRoutinesAt); ++k)
        s.fw.cpu.hook(kRoutinesAt[k], [&s, k](Sh4& c) {
            std::array<uint32_t, 7> more {};
            for (int n = 0; n < kRoutineRegs[k]; ++n) more[(size_t)n] = c.r[5 + n];
            s.rom.push_back(called(kRoutineKinds[k], kRoutinesAt[k], c.r[4], more));
            c.ret(0);
        });
}

namespace {
// A call's message, part and kind as drawn: a part 0…2, a kind 0…2 or 0xff, now and then any
Vc2AssignCall drawTargetNote(std::mt19937& rng) {
    Vc2AssignCall c;
    c.message = messageAt((int)(rng() % kAssignMessages));
    c.part = rng() % 8 ? rng() % 3 : (uint32_t)rng();
    const uint32_t kinds[5] = { 0, 0, 1, 2, rng() % 2 ? 0xffu : (uint32_t)rng() };
    c.kind = kinds[rng() % 5];
    return c;
}

// A few sounding voices made the target's note, some with their age word's bit 29 set, so partners are found and the
// first marked one is told from the last
void aimAtTarget(AssignSides& a, std::mt19937& rng) {
    const std::vector<uint32_t> sounding = voicesOf(a.s, a.s.port.g.lists[V::kSounding]);
    for (int k = 0; k < 4 && !sounding.empty(); ++k) {
        const uint32_t voice = sounding[rng() % sounding.size()];
        Vc2VoiceNode& v = a.s.port.at(voice);
        v.channel = a.s.fw.cpu.rd32(a.target.message);
        v.part = a.target.part;
        v.updated = 0;
        v.state = 0;
        a.s.fw.cpu.wr8(v.note + 0x742, (uint8_t)a.target.kind);
        v.age = voice + 0x20;
        v.order[0] = rng() % 2 ? v.order[0] | 0x20000000 : v.order[0] & ~0x20000000u;
    }
}
} // namespace

// The words the assigner reads beyond the voices' draws: each message's table, the notes' bytes; and the voices' key
// tables, parts, update bits, queues and queue links (all at a case's first); a target note now and then
void assignSteer(AssignSides& a, std::mt19937& rng, bool all) {
    voicesSteer(a.s, rng, all);
    if (all || rng() % 8 == 0) a.target = drawTargetNote(rng);
    for (int i = 0; i < V::kVoices; ++i) {
        Vc2VoiceNode& v = a.s.port.at(a.s.port.addressOf(i));
        if (rng() % 12 == 0) v.queueNext = (uint32_t)rng();
        if (rng() % 12 == 0) v.queuePrev = (uint32_t)rng();
    }
    for (int k = 0; k < kAssignMessages; ++k)
        if (all || rng() % 16 == 0) a.s.fw.cpu.wr32(messageAt(k), tableAt((int)(rng() % kAssignTables)));
    for (uint32_t n = 0; n < 24; ++n)
        if (all || rng() % 16 == 0) a.s.fw.cpu.wr8(kNoteAt + kNoteBytes * n + kAssignNoteOnByte, (uint8_t)rng());
    for (int i = 0; i < V::kVoices; ++i) {
        if (!all && rng() % 6) continue;
        Vc2VoiceNode& v = a.s.port.at(a.s.port.addressOf(i));
        v.channel = rng() % 8 ? tableAt((int)(rng() % kAssignTables)) : (uint32_t)rng();
        v.part = rng() % 8 ? rng() % 3 : (uint32_t)rng();
        v.updated = (uint16_t)(rng() % 4 == 0 ? rng() : 0);
        v.queue = (uint8_t)(rng() % 4);
    }
    if (rng() % 2) aimAtTarget(a, rng);
}

std::string assignStart(AssignSides& a, std::mt19937& rng) {
    std::vector<uint32_t> noise(sizeof a.s.port / 4);
    for (uint32_t& w : noise) w = (uint32_t)rng();
    std::memcpy(&a.s.port, noise.data(), sizeof a.s.port);
    voicesDrawNotes(a.s.fw, rng);
    return voicesInits(a.s);
}

namespace {
// An event's calls counted
void assignCount(AssignSides& a) {
    ++a.n.events;
    a.n.prepared += vc2CountCalls(a.s.mine, kVoicesPrepare);
    a.n.stolen += vc2CountCalls(a.s.mine, kVoicesDestroy);
    for (const VoicesCall& call : a.s.mine) a.n.partners += call.kind == kVoicesPrepare && (call.more[3] & 0xff);
}

void sayAssignCounts(const AssignCounts& n) {
    const auto& e = n.byEvent;
    std::printf("assigner: %ld events: %ld starts and %ld builds (%ld voices prepared, %ld with a partner, %ld "
                "destroys), %ld partners, %ld tests, %ld and %ld retunes, %ld truncations of a note, %ld of a voice, "
                "%ld orders cleared, %ld states set\n", n.events, e[0], e[1], n.prepared, n.partners, n.stolen, e[2],
                e[3], e[4], e[5], e[6], e[7], e[8], e[9]);
}
} // namespace

uint32_t assignNoteOf(const VoicesAnswer& a) { return a.note != 0 ? a.note : kNoteAt; }

Vc2AssignCall assignDrawCall(AssignSides& a, std::mt19937& rng) {
    Vc2AssignCall c = rng() % 2 ? a.target : drawTargetNote(rng);
    c.onByte = rng() % 2 ? rng() % 256 : (uint32_t)rng();
    c.passed = (uint32_t)rng();
    return c;
}

void AssignPortParts::prepare(uint32_t voice, const Vc2AssignCall& call, const Vc2Legato& legato) {
    const VoicesCall slot = voicesSlotCall(s_.fw, kVoicesPrepare, voice, s_.port.at(voice).vtable, kAssignPrepareEntry);
    voicesLog(s_.mine, called(kVoicesPrepare, slot.routine, slot.object,
                              { call.message, call.part, call.onByte, packLegato(legato), call.passed, call.kind, 0 }));
    s_.port.at(voice).note = assignNoteOf(s_.draws.next(s_.draws.port));
}

void AssignPortParts::retune(uint32_t voice, uint32_t message, uint32_t part, uint32_t value) {
    slotCalled(kVoicesRetune, voice, kAssignRetuneEntry, { message, part, value });
}

void AssignPortParts::noteOn(uint32_t note, uint32_t partner, int32_t onByte) {
    voicesLog(s_.mine, called(kVoicesNoteOn, kRoutinesAt[0], note, { partner, (uint32_t)onByte }));
}

void AssignPortParts::startNote(uint32_t note) {
    voicesLog(s_.mine, called(kVoicesStartNote, kRoutinesAt[1], note, {}));
}

void AssignPortParts::truncateOnce(uint32_t note) {
    voicesLog(s_.mine, called(kVoicesTruncateOnce, kRoutinesAt[2], note, {}));
}

int8_t AssignPortParts::onByteOf(uint32_t note) { return (int8_t)s_.fw.cpu.rd8(note + kAssignNoteOnByte); }

void AssignPortParts::waitQueue(uint32_t queue, uint32_t voice) {
    voicesLog(s_.mine, called(kVoicesWait, kRoutinesAt[3], queue, { voice }));
}

void AssignPortParts::keyOff(uint32_t voice) { slotCalled(kVoicesKeyOff, voice, kAssignKeyOffEntry, {}); }

uint32_t AssignPortParts::changed(uint32_t voice, uint32_t part) {
    slotCalled(kVoicesChanged, voice, kAssignChangeEntry, { part });
    return s_.draws.next(s_.draws.port).ended;
}

void AssignPortParts::allNotesOff() { voicesLog(s_.mine, called(kVoicesOs, kAssignNotesOffAt, 0, {})); }

uint32_t assignChangedAnswer(VoicesSides& s) { return s.draws.next(s.draws.rom).ended; }

uint8_t AssignPortParts::partSetting(uint32_t part, uint32_t offset) {
    return s_.fw.cpu.rd8(kAssignSettingsAt + kAssignSettingBytes * part + offset);
}

void AssignPortParts::readParameters(uint32_t part) {
    const uint32_t parameters = kAssignParametersAt + kAssignParameterBytes * part;
    voicesLog(s_.mine, called(kVoicesParameters, kAssignReadAt, parameters, {}));
}

int32_t AssignPortParts::clock() {
    voicesLog(s_.mine, called(kVoicesClock, kAssignClockAt, 0, {}));
    return ms;
}

long vc2AssignPart(VoicesSides& s, const Vc2Run& run) {
    return voicesPart<AssignSides, assignInstallRom, assignStart, assignSteer, assignEvents, assignCount,
                      sayAssignCounts>(s, run, 1);
}
#endif
