#pragma once
// The VC-2 part's notes' task, its pass (v309, step 4.3's thirty-seventh version, item 10's fourteenth part): rom.4's
// loop body 0x8938fc72…0x8938fdac in the task 0x8938fc00 (after its start-up, v304's), given the events its wait
// (0x893017f0, mask 0x3fe) answers, in rom.4's order: 0x002 the MIDI queue 0x89981918 drained (0x893110b0, each message
// to v308's dispatcher); 0x004 the ended scan (v272's 0x89390ee0); 0x008, while 0x89981904 is set, the reset of
// sources 1, 2 and 3 (v308's 0x89390340); 0x010 the schedule 0x893b1710 once per count, the count at 0x89981b60 taken
// and cleared under the interrupts' mask; 0x080 all notes off on source 3 (controller 123) with Auto Note's count of
// other keys cleared and its key let go (0x893ebcf0, with one held) and none; 0x040 Auto Note's note-off (source 4, the
// key at 0x8993d038, velocity 64); 0x020 its note-on (the key at 0x8993d034, velocity 100) but with 0x89981904 set or
// another key down, which ends the pass there; 0x100 the polyphonic aftertouch on source 3; 0x200, with 0x89981904
// clear, a held C4 on source 3 (key 60, velocity 127).  Each on the channel of source 4's part 0 node.  The calls out
// through Vc2NotesTaskParts.  Not in the sound yet.  Bit for bit: xt_vc2settings' twenty-second part.
// HARDWARE_FIRMWARE.md, "The VC-2 part's notes' task".
#include "Vc2AutoNote.h"
#include "Vc2MidiDispatch.h"
#include "Vc2PartNodes.h"
#include <cstdint>

class Vc2NotesTaskParts {
public:
    virtual ~Vc2NotesTaskParts() = default;
    virtual void drainQueue() = 0;            // 0x893110b0 on 0x89981918
    virtual void endedScan() = 0;             // 0x89390ee0
    virtual void reset(int32_t source) = 0;   // 0x89390340
    virtual void schedule() = 0;              // 0x893b1710
    virtual void autoNoteRelease() = 0;       // 0x893ebcf0
    virtual void channelMessage(Vc2MidiHandler h, int32_t source, int32_t channel, uint32_t data1, uint32_t data2) = 0;
};

struct Vc2NotesTaskWorld {
    const uint32_t& noteGate;       // 0x89981904
    Vc2PitchSource& pitch;          // 0x89989748
    uint32_t& scheduled;            // 0x89981b60: the schedule's count
    const uint32_t& autoNoteOn;     // 0x8993d034: the key Auto Note's note-on gives
    const uint32_t& autoNoteOff;    // 0x8993d038: and its note-off
    const Vc2PartNodes& nodes;
    Vc2NotesTaskParts& parts;
};

void vc2NotesTaskPass(Vc2NotesTaskWorld& w, uint32_t events);
