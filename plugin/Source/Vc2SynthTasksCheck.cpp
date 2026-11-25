// xt_vc2synth's tasks' steps (v351, out of the notes part's file, Vc2SynthNotesCheck.cpp, where v349 and v350 began
// them): each of rom.4's three tasks' steps on both sides, then what differs.  An update pass (v349): rom.4's loop body
// from its top 0x89395310 until it comes back there, against Vc2Synth::updatePass.  A notes' task's pass (v350): its
// loop body from 0x8938fc66, against Vc2Synth::notesPass.  A block (v351): the part mixer 0x893b1820 on input frames
// drawn, against Vc2Synth::block.  XT_VC2SYNTH_MAIN only.
#ifdef XT_VC2SYNTH_MAIN
#include "Vc2SynthCheck.h"

namespace {
// [FW] v349: the update task's loop top (its wait's call) and the ticks' word; the registers its prologue leaves, r8
// the OS's lock wait and r9 the interrupts' mask; the events its wait answers; the ticks' step
constexpr uint32_t kUpdateLoopAt = 0x89395310, kTicksAt = 0x8993d080;
constexpr uint32_t kLockWait = 0x893018b0, kInterruptMask = 0xffffff0f;
constexpr uint32_t kUpdateEvent = 1, kTimerEvent = 2, kTicksPast = 8;

// [FW] v350: the notes' task's loop top (its wait's call), its wait's mask, its events' bits; the registers its
// prologue leaves (r8…r12: source 4's nodes' offset, the nodes, the schedule, the interrupts' mask, the task), the
// MIDI queue on its stack (v309's check's) and its drain, the OS's; the note-on gate's word, the schedule's count,
// Auto Note's keys and words (each the part's region)
constexpr uint32_t kNotesLoopAt = 0x8938fc66, kNotesMask = 0x3fe, kNotesFirstBit = 0x002, kNotesLastBit = 0x200;
constexpr uint32_t kNotesQueueAt = 0x89981918, kDrainAt = 0x893110b0;
constexpr uint32_t kNotesRegisters[5] = { 0x300, 0x899888d0, 0x893b1710, kInterruptMask, 0x8998190c };
constexpr uint32_t kGateAt = 0x89981904, kScheduledAt = 0x89981b60;
constexpr uint32_t kAutoNoteKeysAt = 0x8993d034, kAutoNoteAt = 0x8993fe3c;
constexpr uint32_t kScheduledMost = 4;                 // the schedule's count drawn 0…3
constexpr uint32_t kPeriodFrom = 50, kPeriods = 350;   // a sounding note's period drawn, in samples

// [FW] v351: the part mixer's block, its input frames' pointer, the part's frames it writes; the input frames in Vc2Fw's
// RAM, the most a draw shifts their noise down by
constexpr uint32_t kMixerAt = 0x893b1820, kInputPointerAt = 0x8993eadc, kFramesAt = 0x89998e20;
constexpr uint32_t kInputAt = 0x8C171000, kInputShifts = 12;

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
} // namespace

std::string vc2SynthEventDifference(Vc2Fw& fw, Vc2Synth& s, const Vc2SynthPosts& posts) {
    std::string d = vc2SynthStepDifference(fw, s, posts);
    for (int tone = 0; tone < Vc2TonePool::kTones && d.empty(); ++tone) d = vc2SynthToneDifference(fw, s, tone);
    return d;
}

// v349: the ticks a few past the last, the events its wait answers (each half the time); rom.4's loop body with the
// registers its prologue leaves
std::string vc2SynthUpdatePass(Vc2Fw& fw, Vc2Synth& s, std::mt19937& rng, Vc2SynthPosts& posts) {
    posts.updateEvents = (rng() % 2 ? kUpdateEvent : 0) | (rng() % 2 ? kTimerEvent : 0);
    s.ticks = s.time.last + rng() % kTicksPast;
    fw.cpu.wr32(kTicksAt, s.ticks);
    fw.cpu.r[8] = kLockWait;
    fw.cpu.r[9] = kInterruptMask;
    fw.cpu.r[12] = Vc2UpdateQueue::kAt;
    if (!loopBodyRan(fw, kUpdateLoopAt)) return "the update pass faults: " + fw.cpu.error();
    s.updatePass(posts.updateEvents);
    return vc2SynthEventDifference(fw, s, posts);
}

// v350: the events its wait answers (what each side posted the task, which the wait takes, and each bit half the time
// besides); the note-on gate set one time in four for the pass alone (the front's routines set it, 0x8942d686 among
// them, which the part has none of), the schedule's count 0…3 (a timer's), Auto Note's two keys and its note with its
// period (its block's) drawn; rom.4's loop body with the registers its prologue leaves and the MIDI queue on its stack,
// its drain (the OS's mailbox) a bare return as the part's
std::string vc2SynthNotesTaskPass(Vc2Fw& fw, Vc2Synth& s, std::mt19937& rng, Vc2SynthPosts& posts) {
    posts.notesWaited = posts.notes & kNotesMask;   // the wait takes what each side posted the task (equal)
    posts.notes &= ~kNotesMask;
    s.notesEvents &= ~kNotesMask;
    for (uint32_t bit = kNotesFirstBit; bit <= kNotesLastBit; bit <<= 1) posts.notesWaited |= rng() % 2 ? bit : 0;
    s.noteGate = rng() % 4 == 0 ? 1 : 0;
    s.schedule.count = rng() % kScheduledMost;
    for (uint32_t& key : s.autoNoteKeys) key = rng() % 128;
    const bool sounding = rng() % 2 != 0;   // Auto Note sounding a note, its period C4's (else −1 and 0)
    s.autoNote.w.note = sounding ? (int32_t)(rng() % 128) : -1;
    s.autoNote.w.period = sounding ? (float)(kPeriodFrom + rng() % kPeriods) : 0.0f;
    fw.cpu.wr32(kGateAt, s.noteGate);
    fw.cpu.wr32(kScheduledAt, s.schedule.count);
    fw.cpu.write(kAutoNoteKeysAt, s.autoNoteKeys.data(), sizeof s.autoNoteKeys);
    fw.cpu.write(kAutoNoteAt, &s.autoNote.w, sizeof s.autoNote.w);
    fw.cpu.wr32(kVc2Sp, kNotesQueueAt);
    for (size_t k = 0; k < std::size(kNotesRegisters); ++k) fw.cpu.r[8 + k] = kNotesRegisters[k];
    fw.cpu.hook(kDrainAt, [](Sh4& c) { c.ret(0); });   // the OS's mailbox: the part has none, as its drainQueue
    const bool ran = loopBodyRan(fw, kNotesLoopAt);
    fw.cpu.unhook(kDrainAt);
    if (!ran) return "the notes' task's pass faults: " + fw.cpu.error();
    s.notesPass(posts.notesWaited);
    std::string d = vc2SynthEventDifference(fw, s, posts);
    s.noteGate = 0;
    fw.cpu.wr32(kGateAt, 0);
    return d;
}

// v351: the input frames drawn (silence one time in four, else noise shifted down 0…11 bits), rom.4's part mixer into
// the part's frames with its input pointer at them; the part's block given the same frames, its mixer's pointer word
// rom.4's
std::string vc2SynthMixerBlock(Vc2Fw& fw, Vc2Synth& s, std::mt19937& rng, Vc2SynthInput& input,
                               const Vc2SynthPosts& posts) {
    const bool silent = rng() % 4 == 0;
    const uint32_t shift = rng() % kInputShifts;
    for (int32_t& word : input) word = silent ? 0 : (int32_t)rng() >> shift;
    fw.cpu.write(kInputAt, input.data(), sizeof input);
    fw.cpu.wr32(kInputPointerAt, kInputAt);
    if (!fw.call(kMixerAt, { kFramesAt })) return "the block faults: " + fw.cpu.error();
    s.mixer.w.inputAt = kInputAt;
    s.input = input.data();
    s.block();
    s.input = nullptr;
    return vc2SynthEventDifference(fw, s, posts);
}

namespace {
// [FW] v354: the clock's ms on its object (0x89312f50); the clock task's loop top (its wait's call) and the registers its
// prologue leaves: r8 the clock object, r9 a task's own alarm, r10 the take, r11 its wait's mask, r12 its MIDI clock's
// object, r13 the wait
constexpr uint32_t kClockMsAt = 0x89312f50, kClockLoopAt = 0x89313c36, kClockAt = 0x8995cb40;
constexpr uint32_t kClockRegisters[6] = { kClockAt, 0x89301800, 0x89312fd0, Vc2SynthClock::kMask, 0x8995cc08,
                                          0x893017f0 };
constexpr uint32_t kMostMs = 3, kSixMost = 300, kTempoFrom = 100, kTempos = 2900, kTicksAhead = 60;

// The clock's state drawn now and then on both sides: the clocks from outside (one time in four: the switch, its count
// by six clocks and the ms since, the ramp, the clocks seen), the clocks out's switch and counts, the tempo, and the
// ticks run ahead of the take, so the schedule fires often. No waiter: its event would go to a task not the part's.
void clockDrawn(Vc2Fw& fw, Vc2Synth& s, std::mt19937& rng) {
    Vc2ClockObject& o = s.clockObject;
    if (rng() % 4 == 0) {
        o.external = (uint8_t)(rng() % 2);
        o.sync = rng() % 4 == 0 ? 1 : 0;
        o.six = (uint16_t)(1 + rng() % kSixMost);
        o.since = (uint16_t)(rng() % (o.six + 20u));
        o.ramp = (uint16_t)rng();
        o.seen = (int8_t)(rng() % 8 - 1);
    }
    if (rng() % 4 == 0) {
        o.sendOut = (uint16_t)(rng() % 2);
        o.outCount = (int8_t)(1 + rng() % 20);
        o.outPending = (uint16_t)(rng() % 3);
    }
    if (rng() % 8 == 0) o.quarterMs = (int32_t)(kTempoFrom + rng() % kTempos);
    if (rng() % 4 == 0) o.ticks += rng() % kTicksAhead;
    fw.cpu.write(kClockAt, &o, sizeof o);
}
} // namespace

// v354: the clock's state drawn now and then, one to three of its ms on both sides, then the clock task's pass: rom.4's
// loop body with the registers its prologue leaves, its wait taking what each side raised on it (its own alarm's
// event besides half the time), against Vc2Synth::clockPass
std::string vc2SynthClockEvent(Vc2Fw& fw, Vc2Synth& s, std::mt19937& rng, Vc2SynthPosts& posts) {
    clockDrawn(fw, s, rng);
    for (uint32_t k = 1 + rng() % kMostMs; k > 0; --k) {
        if (!fw.call(kClockMsAt, {})) return "the clock's ms faults: " + fw.cpu.error();
        s.clockMs();
    }
    const std::string raised = vc2SynthStepDifference(fw, s, posts);   // what each side raised, before the wait
    if (!raised.empty()) return "after the clock's ms: " + raised;
    posts.clockWaited = (posts.clock & Vc2SynthClock::kMask) | (rng() % 2 ? Vc2SynthClock::kAlarmEvent : 0);
    posts.clock &= ~Vc2SynthClock::kMask;
    s.clockEvents &= ~Vc2SynthClock::kMask;
    for (size_t k = 0; k < std::size(kClockRegisters); ++k) fw.cpu.r[8 + k] = kClockRegisters[k];
    if (!loopBodyRan(fw, kClockLoopAt)) return "the clock task's pass faults: " + fw.cpu.error();
    s.clockPass(posts.clockWaited);
    return vc2SynthEventDifference(fw, s, posts);
}
#endif
