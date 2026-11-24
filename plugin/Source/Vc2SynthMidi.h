#pragma once
// The part's MIDI parts, the part's own (v346, Vc2Synth whole's seventh version): what a message reaches as rom.4 takes
// it, the OS MIDI's handler (0x89314e90, Vc2OsMidi) then the notes' task's dispatcher (0x8938fed0), each call to the
// part's port.  The channel messages by their handler (a table): the note-on and note-off to the channel's (v307's),
// the pressures and the bend its listeners, the controller the part's (0x89392f90), the program change a bare return
// (0x89393090).  A part object's note start and note-off by its class (its +4): the held one's (0x898cd3b8: 0x89396a80,
// 0x89396c90) the assigner's, given the node, its key table, its word, the key and the velocity; the plain one's
// (0x898cd184: 0x89390540, 0x89390550) bare returns.  The parameters' messages by their word's two bytes, the kind and
// the second; Out DC cut for TG (0x893955e0) into the audio task's object at 0x8998a480 (+0x24).  The OS MIDI's
// listeners read from the part's memory (none after the start); the OS's routines a 0x40 or 0x01 message runs are the
// OS's, not the part's.  v350: the notes' task's pass's parts too (v309's vc2NotesTaskPass): the MIDI queue's drain
// none, as the part dispatches a message as it comes; the ended scan (0x89390ee0); a source's reset (0x89390340, the
// dispatcher's way for 0x70); the voices' schedule (0x893b1710, Vc2SynthUpdate's); Auto Note's release (0x893ebcf0);
// a channel message as the dispatcher's.  Bit for bit: xt_vc2synth's notes part.  HARDWARE_FIRMWARE.md, "The part's
// MIDI note path", "The part's notes' task".  The notes' task.
#include "Vc2NotesTask.h"
#include "Vc2OsMidi.h"
#include "Vc2PartChannel.h"
#include "Vc2SynthControls.h"
#include "Vc2SynthSettings.h"

class Vc2SynthMidi final : public Vc2MidiParts,
                           public Vc2ChannelParts,
                           public Vc2OsMidiParts,
                           public Vc2NotesTaskParts {
public:
    explicit Vc2SynthMidi(Vc2Synth& s) : s_(s) {}
    // A message as rom.4 takes it: the OS MIDI's handler, then the dispatcher, each on the message and its word
    void message(const Vc2MidiMessage& m, uint32_t word);
    // The dispatcher's calls: a channel message by its handler, the parameters' messages, DC cut, all notes off
    void channelMessage(Vc2MidiHandler h, int32_t source, int32_t channel, uint32_t data1, uint32_t data2) override;
    void parameters(uint32_t word) override { s_.settings->parameters(kindOf(word), secondOf(word)); }
    void partParameters(uint32_t word) override { s_.settings->partParameters(kindOf(word), secondOf(word)); }
    void dcCut(int32_t value) override { s_.audioTask[Vc2Synth::kDcCutWord] = value != 0 ? 1 : 0; }
    void allNotesOff() override { s_.post(kVc2SynthNotesTask, kVc2SynthAllNotesOff); }
    // The channel messages' calls: a part object's two virtuals by its class, the release over a key table, the
    // listeners
    void objectNoteOn(uint32_t object, uint32_t table, uint32_t word, uint32_t key, uint32_t velocity) override;
    void objectNoteOff(uint32_t object, uint32_t table, uint32_t word) override;
    void releaseTable(uint32_t table) override { s_.assigner.noteOff(table, s_.keys, s_.voices, *s_.voiceParts); }
    void notify(uint32_t state) override { s_.controls->notify(state); }
    // The OS MIDI's: a port's listeners from the part's memory, a listener told, the OS's routines, SysEx's flag
    uint32_t listenerOf(uint32_t node) override;
    uint32_t nextListener(uint32_t node) override;
    void heard(uint32_t, uint32_t, uint32_t, uint32_t) override {}   // no listener of the part's
    void osRoutine(uint32_t, uint32_t) override {}                    // the OS's own, not the part's
    uint8_t sysexOn() override { return s_.sysexOn; }
    // v350: the notes' task's pass's calls beyond a channel message
    void drainQueue() override {}   // no mailbox: `message` dispatches as a message comes
    void endedScan() override;
    void reset(int32_t source) override;
    void schedule() override;
    void autoNoteRelease() override;

private:
    Vc2MidiWorld dispatchWorld() { return { s_.rows, s_.noteGate, *s_.settings, *this }; }
    static uint8_t kindOf(uint32_t word) { return (uint8_t)(word & 0xff); }
    static uint8_t secondOf(uint32_t word) { return (uint8_t)((word >> 8) & 0xff); }
    bool held(uint32_t object) const;   // a part object's class the held one
    Vc2ChannelWorld channelWorld();
    Vc2Synth& s_;
};
