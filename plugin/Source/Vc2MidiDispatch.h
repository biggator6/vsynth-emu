#pragma once
// The VC-2 part's MIDI dispatcher (v308, step 4.3's thirty-sixth version, item 10's thirteenth part): rom.4's
// 0x8938fed0, the notes' task's virtual (in its vtables at 0x898cd168 and 0x898cd180), given each message its mailbox
// holds: four bytes (the status, two data bytes, the source) and a word.  By the status's class (0x8938fe50: a channel
// message's status less its channel, else the status itself): the note-off, the note-on (a velocity of 0 a note-off at
// 64, and none while the word at 0x89981904 is set), the polyphonic aftertouch, the controller, the program change, the
// channel pressure and the pitch bend to their handlers (v307's, v301's controller); the parameters' messages, 0x40 and
// 0x41, their word's first byte its kind, a kind of 3 reading Out DC cut for TG (ID 0x532 on the system's third block)
// into 0x893955e0 first (0x40's by 0x89390230), then v297's parameters 0x89392890 or partParameters 0x89392830 on the
// part (0x899888ac); 0x70, the reset 0x89390340: each of the source's channels given controllers 120, 123 and 121, then
// all notes off, or with a note held (0x899891d0) 121 alone.  The task object it is a virtual of is unread.  The calls
// out through Vc2MidiParts.  Not in the sound yet.  Bit for bit: xt_vc2settings' twenty-first part.  HARDWARE_FIRMWARE.md,
// "The VC-2 part's MIDI dispatcher".  The notes' task.
#include "Vc2PartSettings.h"
#include <cstdint>

// one-off: rom.4's own layout; a declaration, not a job to template
// [FW] a message as the mailbox holds it
struct Vc2MidiMessage {
    uint8_t status = 0, data1 = 0, data2 = 0;
    uint8_t source = 0;   // +3: 1…3 the inputs, 4 Auto Note
};

// [FW] a channel message's handler: the note-off 0x89392e70, note-on 0x89392da0, polyphonic aftertouch 0x893930a0,
// controller 0x89392f90, program change 0x89393090, channel pressure 0x893930c0 and pitch bend 0x893930e0
enum class Vc2MidiHandler : uint8_t { kNoteOff, kNoteOn, kPolyPressure, kController, kProgram, kPressure, kBend };
// [FW] their routines, by Vc2MidiHandler (the checks' hooks', v309; v308's check's own until then)
inline constexpr uint32_t kVc2MidiHandlers[] = { 0x89392e70, 0x89392da0, 0x893930a0, 0x89392f90,
                                                 0x89393090, 0x893930c0, 0x893930e0 };

// What the dispatcher calls
class Vc2MidiParts {
public:
    virtual ~Vc2MidiParts() = default;
    // a handler given the source, channel and data (the program change and pressure their first alone)
    virtual void channelMessage(Vc2MidiHandler h, int32_t source, int32_t channel, uint32_t data1, uint32_t data2) = 0;
    virtual void parameters(uint32_t word) = 0;       // 0x89392890 on the part, given the word
    virtual void partParameters(uint32_t word) = 0;   // 0x89392830
    virtual void dcCut(int32_t value) = 0;            // 0x893955e0: Out DC cut for TG
    virtual void allNotesOff() = 0;                   // 0x89391e30
};

struct Vc2MidiWorld {
    const Vc2PartRows& rows;      // the held note's word (0x899891d0)
    const uint32_t& noteGate;     // 0x89981904: no note-on while set
    Vc2PartParams& params;
    Vc2MidiParts& parts;
};

uint8_t vc2MidiClass(uint8_t status);                                              // 0x8938fe50
void vc2MidiDispatch(Vc2MidiWorld& w, const Vc2MidiMessage& m, uint32_t word);    // 0x8938fed0
