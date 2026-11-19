#pragma once
// rom.4's Matrix set-up (v247), bit for bit: the Matrix's settings, rom.4's object G at 0x89997398, a record a part
// (M's front, Vc2MatrixFront) made from the part's parameters by 0x893a46f0, the system's controller assignments by
// 0x893a50b0, both by 0x893a51c0; M made from its part's record by 0x893a2ee0, with the part's levels from Matrix group 2
// (0x893a2e50 alone), and a note's head by 0x893a2eb0.  A record's sources, each Matrix Control's five parameters from
// ID 0x16a: its source held to the choice's (0 and 32 none, past 109 none), its routine by the choice (0x893a1c70,
// vc2MatrixSourceRoutine), and its two slots by the destination routines 0x893a30d0 and 0x893a36a0 (Vc2MatrixTables,
// tabulated from rom.4's own).  The parameters come through Vc2MatrixParams (the getter 0x89324350 on the readers
// 0x893207b0 and 0x89320790, outside M); 0x893b1ee0 behind Vc2MatrixPart.  Which channel state G names (0x893a5050, by
// Receive Switch and Channel), the static M's initializers (0x893a2fe0, 0x893a3020) and G's allocation (0x893a5030) are
// the part's.  xt_vc2cosm's fourth part.  HARDWARE_FIRMWARE.md, "The VC-2's Matrix set-up".  The tone generator's.
#include "Vc2Matrix.h"
#include <array>
#include <cstdint>

// G: by part its record, the channel state's word M takes, the system's assignments
struct Vc2MatrixSettings {
    static constexpr int kParts = 16;
    Vc2MatrixFront parts[kParts];   // +0x000…+0x83f: a part's record (0x893a46f0), its head Bend Range Up, Down × 1000
    uint32_t channel;               // +0x840: the channel state's address, M's +0x168 (0x893a5050, the part's)
    uint8_t beams[4];               // +0x844…+0x847: Beam 2 and 3 Assign L, R, 0 and 32 as 0xff
    uint8_t assigns[7];             // +0x848…+0x84e: the sources' seven assignments (Vc2MatrixSourceState::assigns)
    uint8_t unread84f;
};
static_assert(sizeof(Vc2MatrixSettings) == 0x850, "rom.4's layout, the Matrix's settings");

// M's words past the core's: the channel state's address (+0x168) and the part's number (+0x16c)
struct Vc2MatrixLinks {
    uint32_t channel;
    int16_t part;
    uint8_t unread16e[2];
};
static_assert(sizeof(Vc2MatrixLinks) == 8, "rom.4's layout, M's +0x168…+0x16f");

// The parameters by ID, as the getter 0x89324350 reads them: on a part's block (the reader 0x893207b0) or the system's
// (0x89320790)
class Vc2MatrixParams {
public:
    virtual ~Vc2MatrixParams() = default;
    virtual int32_t partValue(int part, int16_t id) = 0;
    virtual int32_t systemValue(int16_t id) = 0;
};

// What the levels reach past M: 0x893b1ee0, called when they are new
class Vc2MatrixPart {
public:
    virtual ~Vc2MatrixPart() = default;
    virtual void levelsMoved() = 0;
};

// The part's levels, rom.4's 0x89997290…0x89997298: Matrix group 2's three sums (destinations 9…11) × the scale
struct Vc2MatrixLevels {
    std::array<float, 3> values {};
};

class Vc2MatrixSetup {
public:
    Vc2MatrixSetup(Vc2MatrixParams& params, Vc2MatrixPart& part) : params_(params), part_(part) {}
    // 0x893a46f0: part `part`'s record from its parameters; nothing while `held` (rom.4's 0x8993d0c0), false then
    bool record(Vc2MatrixSettings& g, int part, bool held);
    // 0x893a50b0: the system's assignments
    void assigns(Vc2MatrixSettings& g);
    // 0x893a51c0: the assignments, then the record as 0x893a46f0 makes it
    bool settings(Vc2MatrixSettings& g, int part, bool held);
    // 0x893a2ee0: M from its part's record (links.part) and G's channel word, then the levels
    void init(Vc2MatrixWords& m, Vc2MatrixLinks& links, const Vc2MatrixSettings& g, Vc2MatrixRefresh& refresh);
    // 0x893a2e50: the levels from Matrix group 2's reader
    void levels(Vc2MatrixWords& m, Vc2MatrixRefresh& refresh);
    Vc2MatrixLevels levelsOut;   // rom.4's 0x89997290…

private:
    Vc2MatrixParams& params_;
    Vc2MatrixPart& part_;
};

// M's words from a part's record, as 0x893a2ee0 and a note's M (0x893a5db0, v280) take them: the record's sources,
// uses, slots and routines, then +0x084…+0x165 the zeros' run (rom.4's 0xe2 bytes from 0x895402f6)
void vc2MatrixFromRecord(Vc2MatrixWords& m, const Vc2MatrixFront& rec);
// 0x893a2eb0: a note's velocity, key and poly key into M's head when it is M's part
void vc2MatrixHead(Vc2MatrixWords& m, const Vc2MatrixLinks& links, int32_t part, const std::array<uint8_t, 3>& bytes);
// 0x893a30d0: a patch's destination as M's, its uses bit set; 0 for none (no source, no sensitivity, past 78)
int32_t vc2MatrixDestination(int32_t source, int32_t dest, int32_t sens, uint16_t* uses);
// 0x893a36a0: a slot's depth for its destination and sensitivity; 0 for no source or past 78
int16_t vc2MatrixDepth(uint8_t source, int32_t dest, int32_t sens);
