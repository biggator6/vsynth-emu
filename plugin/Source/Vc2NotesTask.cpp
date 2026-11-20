// The VC-2 part's notes' task, its pass: see Vc2NotesTask.h (v309).  Each event's way a row in rom.4's order of tests,
// the one that ends the pass answering false.
#include "Vc2NotesTask.h"
#include <utility>

namespace {
using H = Vc2MidiHandler;
using W = Vc2NotesTaskWorld;
constexpr int32_t kInputs = 3, kAutoNote = 4;                      // [FW] the sources the task plays on
constexpr uint32_t kNotesOffNumber = 123;                          // [FW] All Notes Off
constexpr uint32_t kOffVelocity = 64, kOnVelocity = 100;           // [FW] Auto Note's note-off and note-on
constexpr uint32_t kC4 = 60, kC4Velocity = 127;                    // [FW] the held C4
constexpr int kAutoNoteNode = Vc2PartNodes::kParts * (kAutoNote - 1);   // [FW] source 4's part 0 node, 0x89988bd0

int32_t autoNoteChannel(const W& w) { return w.nodes.nodes[kAutoNoteNode].channel; }

using Way = bool (*)(W&);

bool taskQueue(W& w) {
    w.parts.drainQueue();
    return true;
}

bool taskEnded(W& w) {
    w.parts.endedScan();
    return true;
}

bool resetInputs(W& w) {
    if (w.noteGate == 0) return true;
    for (int32_t source = 1; source <= kInputs; ++source) w.parts.reset(source);
    return true;
}

bool taskSchedule(W& w) {
    auto count = static_cast<int32_t>(w.scheduled);
    w.scheduled = 0;
    for (; count > 0; --count) w.parts.schedule();
    return true;
}

bool inputsNotesOff(W& w) {
    w.parts.channelMessage(H::kController, kInputs, autoNoteChannel(w), kNotesOffNumber, 0);
    w.pitch.others = 0;
    if (w.pitch.note >= 0) w.parts.autoNoteRelease();
    w.pitch.note = -1;
    return true;
}

bool autoNoteOff(W& w) {
    w.parts.channelMessage(H::kNoteOff, kAutoNote, autoNoteChannel(w), w.autoNoteOff, kOffVelocity);
    return true;
}

// Not with the gate set or another key down, which ends the pass
bool autoNoteOn(W& w) {
    if (w.noteGate != 0 || w.pitch.others > 0) return false;
    w.parts.channelMessage(H::kNoteOn, kAutoNote, autoNoteChannel(w), w.autoNoteOn, kOnVelocity);
    return true;
}

bool taskPressure(W& w) {
    w.parts.channelMessage(H::kPolyPressure, kInputs, autoNoteChannel(w), 0, 0);
    return true;
}

bool heldC4(W& w) {
    if (w.noteGate != 0) return false;
    w.parts.channelMessage(H::kNoteOn, kInputs, autoNoteChannel(w), kC4, kC4Velocity);
    return true;
}

// [FW] the events in rom.4's order of tests
constexpr std::pair<uint32_t, Way> kWays[] = {
    { 0x002, taskQueue },   { 0x004, taskEnded },  { 0x008, resetInputs },  { 0x010, taskSchedule },
    { 0x080, inputsNotesOff }, { 0x040, autoNoteOff }, { 0x020, autoNoteOn }, { 0x100, taskPressure },
    { 0x200, heldC4 },
};
} // namespace

void vc2NotesTaskPass(Vc2NotesTaskWorld& w, uint32_t events) {
    for (const auto& [bit, way] : kWays)
        if ((events & bit) != 0 && !way(w)) return;
}
