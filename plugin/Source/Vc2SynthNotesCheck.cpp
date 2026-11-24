// xt_vc2synth's notes part (v346): the part's MIDI note path against rom.4's own, after the load part and before the
// reads part, on the seed three after the run's.  [cases] cases (16 at the least), each a patch loaded on both sides
// (a store of noise or none, the part's read, the system's dispatchers and part 0's message, its node the held class;
// v349: a sounding tone's VOCODER OSC keeping its Type, vocoderTypesKept; robot human depth 0 one time in 4),
// then 40 MIDI messages, each as rom.4 takes it: the OS MIDI's handler 0x89314e90 then the notes' task's dispatcher
// 0x8938fed0, each given (0, the message, its word), on rom.4's side; `Vc2Synth::midiMessage` on the part's.  A
// message: a note-on (two in five: a key of a few near the middle, a velocity 1…127, now and then 0), a note-off (a key
// one of the same few), a controller (one the part has a class for, or any number below 128; a value below 128), a
// bend, either pressure or a program change; on source 1 mostly (2, 3 now and then), the channel part 0 receives on
// mostly (any now and then); or one in six a parameters' message (0x40, 0x41: its word a kind 0…13, 3 with Out DC cut
// one time in four, and a second byte) or the reset (0x70), the OS's routines these run on rom.4's side
// (Vc2OsMidiParts::kOsRoutines) hooked as the part leaves them; v349: one event in 13 an update pass instead
// (updatePassEvent).  After each every region of the part, the vocoder, the flags posted the notes' task and each
// tone's words.  XT_VC2SYNTH_MAIN only.
#ifdef XT_VC2SYNTH_MAIN
#include "Vc2Params.h"
#include "Vc2PatchStore.h"
#include "Vc2SynthCheck.h"
#include "Vc2SynthMidi.h"
#include <cstdlib>

namespace {
constexpr uint32_t kHandler = 0x89314e90, kDispatcher = 0x8938fed0;   // [FW]
constexpr uint32_t kMessageAt = 0x8C170000, kWordAt = 0x8C170010;      // in Vc2Fw's RAM: a message, its word
constexpr int kEvents = 40, kKeys = 6, kWordKinds = 14;
// The controllers drawn mostly: those the part has a class for (Vc2PartNumbers' rows) and the modulation wheel
constexpr uint8_t kControllers[] = { 1, 5, 6, 7, 10, 11, 38, 64, 65, 66, 68, 71, 72, 73, 74, 84, 91, 93,
                                     120, 121, 123, 124, 125, 126, 127 };
// A message's kind by a draw of 24: the note-on, note-off, controller, bend, poly pressure, pressure, program change,
// the parameters' message, the part's, the reset
constexpr std::array<uint8_t, 24> kKindByDraw { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 3, 4, 5, 6, 7, 7, 8,
                                                9 };
constexpr uint8_t kClasses[] = { 0x90, 0x80, 0xb0, 0xe0, 0xa0, 0xd0, 0xc0, 0x40, 0x41, 0x70 };   // [FW] by kind
constexpr size_t kWordFrom = 7, kResetKind = 9;   // the kinds given a word, the reset's

// [FW] v349: the update task's loop top (its wait's call), the ticks' word; the registers its prologue leaves (r8 the
// OS's lock wait, r9 the interrupts' mask), the events its wait answers
constexpr uint32_t kUpdateLoopAt = 0x89395310, kTicksAt = 0x8993d080, kLockWait = 0x893018b0;
constexpr uint32_t kInterruptMask = 0xffffff0f, kUpdateEvent = 1, kTimerEvent = 2;
constexpr uint32_t kPassOneIn = 13, kTicksPast = 8;   // an event each task's pass one time in 13; the ticks' step

// [FW] v350: the notes' task's loop top (its wait's call), its wait's mask, its events' bits; the registers its
// prologue leaves (r8…r12: source 4's nodes' offset, the nodes, the schedule, the interrupts' mask, the task), the
// MIDI queue on its stack (v309's check's); the note-on gate's word and the schedule's count (each the part's region)
constexpr uint32_t kNotesLoopAt = 0x8938fc66, kNotesMask = 0x3fe, kNotesFirstBit = 0x002, kNotesLastBit = 0x200;
constexpr uint32_t kNotesQueueAt = 0x89981918, kDrainAt = 0x893110b0;   // the MIDI queue and its drain, the OS's
constexpr uint32_t kNotesRegisters[5] = { 0x300, 0x899888d0, 0x893b1710, kInterruptMask, 0x8998190c };
constexpr uint32_t kGateAt = 0x89981904, kScheduledAt = 0x89981b60, kAutoNoteKeysAt = 0x8993d034;
constexpr uint32_t kScheduledMost = 4;   // the schedule's count drawn 0…3
constexpr uint32_t kAutoNoteAt = 0x8993fe3c;   // [FW] Auto Note's words: its note, its period (the block's)
constexpr uint32_t kPeriodFrom = 50, kPeriods = 350;   // a sounding note's period drawn, in samples

struct NotesCounts {
    long events = 0, voiced = 0;   // voiced: the messages after which a note's age is new (a voice started)
    long updates = 0;              // v349: the update passes among the events
    long notesPasses = 0;          // v350: the notes' task's passes among them
    long under = 0, kept = 0;   // v349: the patches drawn under a VOCODER's tone, the Types kept (vocoderTypesKept)
    std::array<long, std::size(kClasses)> byKind {};
};

// A message drawn, and its word: its status's class by kind, its data, its source; the parameters' messages a word of
// a kind 0…13 (3, the system's, with Out DC cut, one time in four) and its second byte (0 half the time)
struct Drawn {
    Vc2MidiMessage m;
    uint32_t word = 0;
};
Drawn messageDrawn(std::mt19937& rng, NotesCounts& n, uint8_t receiving) {
    const size_t kind = kKindByDraw[rng() % kKindByDraw.size()];
    ++n.byKind[kind];
    const uint8_t channel = rng() % 8 == 0 || kind >= kWordFrom ? (uint8_t)(rng() % 16) : receiving;
    const uint8_t key = (uint8_t)(57 + 3 * (rng() % kKeys));
    const uint8_t number = rng() % 4 ? kControllers[rng() % std::size(kControllers)] : (uint8_t)(rng() % 128);
    Drawn d;
    d.m.status = (uint8_t)(kClasses[kind] | (kind < kWordFrom ? channel : 0));
    d.m.data1 = kind <= 1 ? key : kind == 2 ? number : (uint8_t)(rng() % 128);
    d.m.data2 = (uint8_t)(rng() % 128);
    if (kind == 0 && d.m.data2 == 0 && rng() % 4) d.m.data2 = (uint8_t)(1 + rng() % 127);
    d.m.source = rng() % 8 == 0 ? (uint8_t)(2 + rng() % 2) : 1;
    const uint32_t wordKind = rng() % 4 == 0 ? 3 : rng() % kWordKinds, second = rng() % 2 ? 0 : rng() % 256;
    if (kind >= kWordFrom && kind != kResetKind) d.word = wordKind | second << 8;
    return d;
}

// [FW] v349: OSC 1's Type (OSC 2's 52 IDs on), its VOCODER, and the kind an OSC voice of VOCODER holds; robot human
// depth (the part's block)
constexpr int kOscTypeId = 0x2e5, kOscTypeStep = 52, kOscs = 2, kRobotId = 0x1a3;
constexpr uint32_t kVocoderType = 3, kVocoderKind = 5, kRobotZeroOneIn = 4;

// An ID's bits in the store at a zone set to `raw`, where xtStoreRaw reads them (its value raw less the centre)
void rawPut(std::vector<uint8_t>& store, int id, int zone, uint32_t raw) {
    const XtParamDesc* d = vc2ParamDesc(id);
    const int section = xtStoreSection(vc2Store(), id, zone);
    if (d == nullptr || section < 0) return;
    const int width = std::abs(d->width), total = width + d->bit, bytes = (total + 7) / 8, at = section + d->offset;
    if (at < 0 || at + bytes > (int)store.size() || width >= 32) return;
    const int shift = bytes * 8 - total;
    const uint32_t mask = ((1u << width) - 1) << shift;
    uint32_t v = 0;
    for (int k = 0; k < bytes; ++k) v = v << 8 | store[(size_t)(at + k)];
    v = (v & ~mask) | (raw << shift & mask);
    for (int k = bytes - 1; k >= 0; --k, v >>= 8) store[(size_t)(at + k)] = (uint8_t)v;
}

// v349's steer, the user's call (2026-11-24, "Defer it"; VC2_MODE_PLAN.md's [deferred] item on the VOCODER's NaN
// sign): a new patch keeps an OSC's Type at VOCODER in the kind of a tone on the sounding list (its id's high half)
// whose OSC voice there is VOCODER's (kind 5), and is a store then.  Changed under such a tone (to PCM), the update
// pass converts its note's block by the new type, PCM's converter writing −1 at P+0x48 (the block's +0xf8), which kind
// 5's update 0x893afff0 reads as its balance envelope depth: a NaN whose sign the interpreter's FSUB keeps and
// xtf::sub flips.  Every other type change, and every other note, is drawn as before.  Whether such a tone sounds;
// `changed` counts the Types it set back to VOCODER
bool vocoderTypesKept(Vc2Synth& s, std::vector<uint8_t>& store, long& changed) {
    bool under = false;
    int n = 0;
    for (uint32_t at = s.lists.sounding; at != 0 && n < Vc2TonePool::kTones; at = s.tones.at(at).head.next, ++n) {
        const int kind = (int)(s.tones.at(at).head.id >> 16);
        for (int osc = 0; osc < kOscs; ++osc) {
            if (s.hostOf(at).oscs[(size_t)osc].kind() != kVocoderKind) continue;
            const int id = kOscTypeId + kOscTypeStep * osc;
            under = true;
            if (xtStoreValue(vc2Store(), store, id, kind) == (int)kVocoderType) continue;
            const XtParamDesc* d = vc2ParamDesc(id);
            rawPut(store, id, kind, kVocoderType + (uint32_t)(d ? d->centre : 0));
            ++changed;
        }
    }
    return under;
}

// v349: robot human depth 0 one time in 4 (v350; 8 until then), so that the pitch source's post (0x89394a60) is seen on
// Auto Note's On Switch alone, its depth 0 (the depth is read with an EXTIN OSC, Auto Note's switch too; noise gives 0
// one time in 128)
void robotDepthDrawn(std::vector<uint8_t>& store, std::mt19937& rng) {
    if (rng() % kRobotZeroOneIn != 0) return;
    const XtParamDesc* d = vc2ParamDesc(kRobotId);
    rawPut(store, kRobotId, 0, (uint32_t)(d ? d->centre : 0));
}

// The channel part 0's node of source 1 receives on (0 when none, or every one): where the notes go mostly
uint8_t receivingOf(const Vc2Synth& s) {
    const int8_t c = s.nodes.of(1, 0).channel;
    return c < 0 ? 0 : (uint8_t)c;
}

// After a step: every region, the vocoder, the events, each tone's words
std::string partStepDifference(Vc2Fw& fw, Vc2Synth& s, const Vc2SynthPosts& posts) {
    std::string d = vc2SynthStepDifference(fw, s, posts);
    for (int tone = 0; tone < Vc2TonePool::kTones && d.empty(); ++tone) d = vc2SynthToneDifference(fw, s, tone);
    return d;
}

// One message on both sides, then what differs
std::string partMidiEvent(Vc2Fw& fw, Vc2Synth& s, const Drawn& m, const Vc2SynthPosts& posts) {
    fw.cpu.write(kMessageAt, &m.m, sizeof m.m);
    fw.cpu.wr32(kWordAt, m.word);
    if (!fw.call(kHandler, { 0, kMessageAt, kWordAt })) return "the OS MIDI's handler faults: " + fw.cpu.error();
    if (!fw.call(kDispatcher, { 0, kMessageAt, kWordAt })) return "the dispatcher faults: " + fw.cpu.error();
    s.midiMessage(m.m, m.word);
    return partStepDifference(fw, s, posts);
}

// A task's loop body on rom.4's side, from its top until it comes back there (v309's way), with the registers its
// prologue left already set; whether it ran without a fault
bool loopBodyRan(Vc2Fw& fw, uint32_t top) {
    uint32_t back = 0;
    int entries = 0;
    fw.cpu.hook(top, [&back, &entries](Sh4& m) {
        if (entries++ == 0) back = m.pr;
        else m.pc = back;
    });
    const bool ran = fw.call(top, {});
    fw.cpu.unhook(top);
    return ran;
}

// v349: one update pass on both sides, then what differs: the ticks a few past the last, the events its wait answers
// (each half the time); rom.4's loop body with the registers its prologue leaves
std::string updatePassEvent(Vc2Fw& fw, Vc2Synth& s, std::mt19937& rng, Vc2SynthPosts& posts) {
    posts.updateEvents = (rng() % 2 ? kUpdateEvent : 0) | (rng() % 2 ? kTimerEvent : 0);
    s.ticks = s.time.last + rng() % kTicksPast;
    fw.cpu.wr32(kTicksAt, s.ticks);
    fw.cpu.r[8] = kLockWait;
    fw.cpu.r[9] = kInterruptMask;
    fw.cpu.r[12] = Vc2UpdateQueue::kAt;
    if (!loopBodyRan(fw, kUpdateLoopAt)) return "the update pass faults: " + fw.cpu.error();
    s.updatePass(posts.updateEvents);
    return partStepDifference(fw, s, posts);
}

// v350: one notes' task's pass on both sides, then what differs: the events its wait answers (what each side posted
// the task, which the wait takes, and each bit half the time besides); the note-on gate set one time in four for the pass alone (the front's routines set it, 0x8942d686 among
// them, which the part has none of), the schedule's count 0…3 (a timer's), Auto Note's two keys and its note with its
// period (its block's) drawn; rom.4's loop body with the registers its prologue leaves and the MIDI queue on its stack,
// its drain (the OS's mailbox) a bare return as the part's
std::string notesPassEvent(Vc2Fw& fw, Vc2Synth& s, std::mt19937& rng, Vc2SynthPosts& posts) {
    posts.notesWaited = posts.notes & kNotesMask;   // the wait takes what each side posted the task (equal)
    posts.notes &= ~kNotesMask;
    s.notesEvents &= ~kNotesMask;
    for (uint32_t bit = kNotesFirstBit; bit <= kNotesLastBit; bit <<= 1) posts.notesWaited |= rng() % 2 ? bit : 0;
    s.noteGate = rng() % 4 == 0 ? 1 : 0;
    s.scheduled = rng() % kScheduledMost;
    for (uint32_t& key : s.autoNoteKeys) key = rng() % 128;
    const bool sounding = rng() % 2 != 0;   // Auto Note sounding a note, its period C4's (else −1 and 0)
    s.autoNote.w.note = sounding ? (int32_t)(rng() % 128) : -1;
    s.autoNote.w.period = sounding ? (float)(kPeriodFrom + rng() % kPeriods) : 0.0f;
    fw.cpu.wr32(kGateAt, s.noteGate);
    fw.cpu.wr32(kScheduledAt, s.scheduled);
    fw.cpu.write(kAutoNoteKeysAt, s.autoNoteKeys.data(), sizeof s.autoNoteKeys);
    fw.cpu.write(kAutoNoteAt, &s.autoNote.w, sizeof s.autoNote.w);
    fw.cpu.wr32(kVc2Sp, kNotesQueueAt);
    for (size_t k = 0; k < std::size(kNotesRegisters); ++k) fw.cpu.r[8 + k] = kNotesRegisters[k];
    fw.cpu.hook(kDrainAt, [](Sh4& c) { c.ret(0); });   // the OS's mailbox: the part has none, as its drainQueue
    const bool ran = loopBodyRan(fw, kNotesLoopAt);
    fw.cpu.unhook(kDrainAt);
    if (!ran) return "the notes' task's pass faults: " + fw.cpu.error();
    s.notesPass(posts.notesWaited);
    std::string d = partStepDifference(fw, s, posts);
    s.noteGate = 0;
    fw.cpu.wr32(kGateAt, 0);
    return d;
}

// An event of the notes part: an update pass or a notes' task's pass one time in 13 each, else a message
std::string notesStepEvent(Vc2Fw& fw, Vc2Synth& s, std::mt19937& rng, Vc2SynthPosts& posts, NotesCounts& n) {
    const uint32_t pick = rng() % kPassOneIn;
    n.updates += pick == 0 ? 1 : 0;
    n.notesPasses += pick == 1 ? 1 : 0;
    if (pick == 0) return updatePassEvent(fw, s, rng, posts);
    if (pick == 1) return notesPassEvent(fw, s, rng, posts);
    return partMidiEvent(fw, s, messageDrawn(rng, n, receivingOf(s)), posts);
}
} // namespace

long vc2SynthNotesPart(Vc2Fw& fw, Vc2Synth& s, const Vc2Run& run, Vc2SynthPosts& posts) {
    NotesCounts n;
    const auto oneCase = [&](std::mt19937& rng, std::vector<uint8_t>& store) {
        const auto kept = [&s, &n, &rng](std::vector<uint8_t>& bytes) {
            robotDepthDrawn(bytes, rng);
            const bool k = vocoderTypesKept(s, bytes, n.kept);
            n.under += k ? 1 : 0;
            return k;
        };
        std::string d = vc2SynthPatchLoaded(fw, s, store, rng, posts, kept);
        for (int e = 0; e < kEvents && d.empty(); ++e) {
            ++n.events;
            const uint32_t age = s.voices.ages;
            d = notesStepEvent(fw, s, rng, posts, n);
            n.voiced += s.voices.ages != age && s.voices.ages != 0 ? 1 : 0;
        }
        return d;
    };
    const long different = vc2SynthCases(fw, s, run, 3, "the notes", oneCase);
    std::printf("synth notes: %d cases, %ld events: %ld note-ons, %ld note-offs, %ld controllers, %ld bends, %ld poly "
                "pressures, %ld pressures, %ld programs, %ld parameters', %ld the part's, %ld resets, %ld update "
                "passes, %ld notes' task's passes; %ld voiced; %ld patches under a VOCODER's tone, %ld Types kept\n",
                vc2SynthCaseCount(run), n.events, n.byKind[0], n.byKind[1], n.byKind[2], n.byKind[3], n.byKind[4],
                n.byKind[5], n.byKind[6], n.byKind[7], n.byKind[8], n.byKind[9], n.updates, n.notesPasses, n.voiced,
                n.under, n.kept);
    return different;
}
#endif
