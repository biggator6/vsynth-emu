// xt_vc2settings' twenty-second part (v309): the notes' task's pass, vc2NotesTaskPass, against rom.4's own loop body,
// run from the loop's top (0x8938fc66) until it comes back there, the way XtFxCheckHand.cpp leaves a task at its wait,
// with the registers the task's prologue leaves and the MIDI queue's address on its stack.  On the seed twenty-one
// after the run's.  Hooked: the wait (0x893017f0, answering the events drawn), and to log their arguments and routine
// the queue's drain, the ended scan, the reset, the schedule, Auto Note's release and the four handlers the pass calls.
// Each case the tables noise; then 400 passes, each on events drawn (each of the nine bits half the time; now and then
// none or any word) with the note-on gate 0, 1 or any, the pitch source's key −1 or a key and its count of other keys
// 0…2 or about it, the schedule's count 0…3, −1 or any below 6, Auto Note's two keys, and source 4's part 0 node's
// channel −1, 0…15 or any.  After each the tables (the nodes among them), the pitch source, the schedule's count and
// the calls.  XT_VC2SETTINGS_MAIN only.
#ifdef XT_VC2SETTINGS_MAIN
#include "Vc2NotesTask.h"
#include "Vc2PartControllersCheck.h"

namespace {
constexpr uint32_t kTaskLoopAt = 0x8938fc66, kTaskWaitAt = 0x893017f0, kWaitMask = 0x3fe;    // [FW]
constexpr uint32_t kQueueAt = 0x89981918, kTaskAt = 0x8998190c;                                // [FW]
constexpr uint32_t kTaskGateAt = 0x89981904, kPitchAt = 0x89989748, kScheduledAt = 0x89981b60;   // [FW]
constexpr uint32_t kOnKeyAt = 0x8993d034, kOffKeyAt = 0x8993d038;                              // [FW]
// [FW] the registers the task's prologue leaves, r8…r12: source 4's nodes' offset, the nodes, the schedule, the
// interrupts' mask, the task
constexpr uint32_t kTaskRegisters[5] = { 0x300, 0x899888d0, 0x893b1710, 0xffffff0f, kTaskAt };
// [FW] the routines the pass calls besides the handlers
constexpr uint32_t kDrainAt = 0x893110b0, kEndedAt = 0x89390ee0, kResetAt = 0x89390340, kScheduleAt = 0x893b1710;
constexpr uint32_t kReleaseAt = 0x893ebcf0;
constexpr int kAutoNoteNode = Vc2PartNodes::kParts * 3;

SettingsCall taskWaited(Sh4& c) { return { kOutTaskWait, 0, 0, 0, (int32_t)c.r[4] }; }
uint32_t eventsDrawn(SettingsSides& s) { return s.taskEvents; }
SettingsCall bareCalled(Sh4&) { return { kOutMidi, 0, 0, 0, 0 }; }
SettingsCall drainCalled(Sh4& c) { return { kOutMidi, 0, 0, 0, (int32_t)c.r[4] }; }
SettingsCall resetCalled(Sh4& c) { return { kOutMidi, (int32_t)c.r[5], 0, 0, (int32_t)c.r[4] }; }

constexpr auto kTaskCallees = [] {
    constexpr auto routine = settingsCalledRoutine;
    std::array<SettingsCallee, 10> a { {
        { kTaskWaitAt, taskWaited, eventsDrawn },      { kDrainAt, drainCalled, nullptr, routine },
        { kEndedAt, bareCalled, nullptr, routine },    { kResetAt, resetCalled, nullptr, routine },
        { kScheduleAt, bareCalled, nullptr, routine }, { kReleaseAt, bareCalled, nullptr, routine },
    } };
    for (size_t k = 0; k < 4; ++k) a[6 + k] = { kVc2MidiHandlers[k], settingsCalledFour, nullptr, routine };
    return a;
}();

void hookTask(SettingsSides& s) { settingsHookCallees(s, kTaskCallees); }

// The port's parts: each call logged as its hook logs it
class NotesTaskPortParts final : public Vc2NotesTaskParts {
public:
    explicit NotesTaskPortParts(std::vector<SettingsCall>& log) : log_(log) {}
    void drainQueue() override { settingsLogRoutine(log_, kDrainAt, { 0, 0, 0, (int32_t)kQueueAt }); }
    void endedScan() override { settingsLogRoutine(log_, kEndedAt, {}); }
    void reset(int32_t source) override { settingsLogRoutine(log_, kResetAt, { source, 0, 0, (int32_t)kTaskAt }); }
    void schedule() override { settingsLogRoutine(log_, kScheduleAt, {}); }
    void autoNoteRelease() override { settingsLogRoutine(log_, kReleaseAt, {}); }
    void channelMessage(Vc2MidiHandler h, int32_t source, int32_t channel, uint32_t d1, uint32_t d2) override {
        settingsLogRoutine(log_, kVc2MidiHandlers[(size_t)h], { channel, (int32_t)d1, (int32_t)d2, source });
    }

private:
    std::vector<SettingsCall>& log_;
};

std::array<Vc2Region, 5> taskRegions(SettingsSides& s) {
    return { { { kPitchAt, &s.pitch, sizeof s.pitch, "the pitch source" },
               { kScheduledAt, &s.scheduled, sizeof s.scheduled, "the schedule's count" },
               { kTaskGateAt, &s.noteGate, sizeof s.noteGate, "the note-on gate" },
               { kOnKeyAt, &s.autoNoteOn, sizeof s.autoNoteOn, "Auto Note's note-on key" },
               { kOffKeyAt, &s.autoNoteOff, sizeof s.autoNoteOff, "Auto Note's note-off key" } } };
}

// The events, the gate, the pitch source, the schedule's count, Auto Note's keys and source 4's part 0 node's channel
void drawTask(SettingsSides& s, std::mt19937& rng) {
    uint32_t events = 0;
    for (uint32_t bit = 0x002; bit <= 0x200; bit <<= 1)
        if (rng() % 2) events |= bit;
    const uint32_t eventPicks[3] = { events, 0, (uint32_t)rng() };
    s.taskEvents = eventPicks[rng() % 8 == 0 ? 1 + rng() % 2 : 0];
    const uint32_t gates[4] = { 0, 0, 1, (uint32_t)rng() };
    s.noteGate = gates[rng() % 4];
    s.pitch.note = rng() % 4 == 0 ? -1 : (int32_t)(rng() % 128);
    const int32_t others[4] = { 0, 1, 2, (int32_t)(rng() % 8) - 2 };
    s.pitch.others = others[rng() % 4];
    const uint32_t counts[4] = { rng() % 4, 0xffffffff, rng() % 6, 0 };
    s.scheduled = counts[rng() % 4];
    s.autoNoteOn = rng() % 4 ? rng() % 128 : (uint32_t)rng();
    s.autoNoteOff = rng() % 4 ? rng() % 128 : (uint32_t)rng();
    const int8_t channels[3] = { -1, (int8_t)(rng() % 16), (int8_t)rng() };
    s.nodes.nodes[kAutoNoteNode].channel = channels[rng() % 3];
}

std::string taskEvent(SettingsSides& s, std::mt19937& rng) {
    drawTask(s, rng);
    settingsWriteRegions(s, taskRegions(s));
    s.fw.cpu.write(Vc2PartNodes::kNodesAt, s.nodes.nodes.data(), sizeof s.nodes.nodes);
    s.fw.cpu.wr32(kVc2Sp, kQueueAt);
    for (size_t k = 0; k < std::size(kTaskRegisters); ++k) s.fw.cpu.r[8 + k] = kTaskRegisters[k];
    // one-off: the pass alone, entered at the loop's top and left at its return there (XtFxCheckHand's way)
    uint32_t back = 0;
    bool entered = false;
    s.fw.cpu.hook(kTaskLoopAt, [&back, &entered](Sh4& m) {
        if (entered) m.pc = back;
        back = entered ? back : m.pr;
        entered = true;
    });
    ++s.n.byEvent[91];
    const std::string what = settingsOnBoth(s, { kTaskLoopAt, "the notes' task's pass", nullptr }, {}, [&] {
        s.mine.log.push_back({ kOutTaskWait, 0, 0, 0, (int32_t)kWaitMask });
        NotesTaskPortParts parts { s.mine.log };
        Vc2NotesTaskWorld w { s.noteGate, s.pitch, s.scheduled, s.autoNoteOn, s.autoNoteOff, s.nodes, parts };
        vc2NotesTaskPass(w, s.taskEvents);
        return 0u;
    });
    s.fw.cpu.unhook(kTaskLoopAt);
    settingsCountCalls(s, { { kOutMidiRoutine, 92 } });
    return what.empty() ? settingsRegionsDifference(s, taskRegions(s)) : what;
}

const SettingsEvent kTaskEvents[] = { taskEvent };
constexpr SettingsSaid kTaskSaid[] = { { 91, "passes" }, { 92, "calls out" } };
} // namespace

const SettingsPartRow kTaskPartRow { 21, &SettingsCounts::taskEvents, &SettingsCounts::taskCalls,
                                     settingsCaseOf<kTaskEvents, sizeof(Vc2PartRows), 5>, nullptr, hookTask,
                                     "notes' task", kTaskSaid };
#endif
