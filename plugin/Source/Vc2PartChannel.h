#pragma once
// The VC-2 part's channel messages (v307, step 4.3's thirty-fifth version, item 10's twelfth part): what the MIDI
// dispatcher 0x8938fed0 calls for a source's channel message: rom.4's note-on 0x89392da0 (a velocity of 0 is the
// dispatcher's note-off), note-off 0x89392e70, and the polyphonic aftertouch 0x893930a0, channel pressure 0x893930c0
// and pitch bend 0x893930e0, one code: the channel's state to the listeners, the data unread.  The program change
// 0x89393090 is a bare return, not ported.  Auto Note's notes come as source 4, on source 3's key table and lists,
// through the pitch source 0x89989748 (v226's Vc2PitchSource): Auto Note's key while no other key is down, the other
// keys counted, and another key releasing Auto Note's note when its On Switch (0x8993fe50) is 1.  The part objects'
// two virtuals (their vtable's +0x0c, the held class's note start 0x89396a80, and +0x18, its note-off 0x89396c90:
// v278's), v278's note-off over a key table 0x89390c00 and the listeners 0x89390ea0 (v305's) through Vc2ChannelParts.
// And a channel's state, key table and nodes (v303's, shared from v307).  Not in the sound yet.  Bit for bit:
// xt_vc2settings' twentieth part.  HARDWARE_FIRMWARE.md, "The VC-2 part's channel messages".  The notes' task.
#include "Vc2AutoNote.h"
#include "Vc2Keys.h"
#include "Vc2PartNodes.h"
#include <cstdint>
#include "XtFn.h"

// What the channel's messages reach beyond the keys, the nodes and the pitch source
class Vc2ChannelParts {
public:
    virtual ~Vc2ChannelParts() = default;
    // a node's part object's vtable (its +4) +0x0c: the note's start, given the key table, the node's word, the key
    // and the velocity
    virtual void objectNoteOn(uint32_t object, uint32_t table, uint32_t word, uint32_t key, uint32_t velocity) = 0;
    virtual void objectNoteOff(uint32_t object, uint32_t table, uint32_t word) = 0;   // its +0x18
    virtual void releaseTable(uint32_t table) = 0;   // 0x89390c00
    virtual void notify(uint32_t state) = 0;         // 0x89390ea0
};

struct Vc2ChannelWorld {
    Vc2Keys& keys;
    const Vc2PartNodes& nodes;
    Vc2PitchSource& pitch;           // 0x89989748
    const int32_t& autoNoteSwitch;   // 0x8993fe50: Auto Note's On Switch
    Vc2ChannelParts& parts;
};

void vc2ChannelNoteOn(Vc2ChannelWorld& w, int32_t source, int32_t channel, uint32_t key, uint32_t velocity);
void vc2ChannelNoteOff(Vc2ChannelWorld& w, int32_t source, int32_t channel, uint32_t key, uint32_t velocity);
void vc2ChannelHeard(Vc2ChannelWorld& w, int32_t source, int32_t channel);   // the three that tell the listeners

// A source's (1…4) channel's state and key table, and each node on its list in turn (v303's, shared from v307)
uint32_t vc2ChannelStateOf(int32_t source, int32_t channel);
uint32_t vc2ChannelTableOf(int32_t source, int32_t channel);
void vc2EachNode(const Vc2PartNodes& nodes, int32_t source, int32_t channel,
                 const XtFn<void(const Vc2PartNode&)>& each);   // v455: XtFn, no allocation
