#pragma once
// What each kind of OSC voice does in each of the frame's routines (v230): rom.4's switches on the voice's kind (+0x54)
// as one row a kind (Vc2OscVoiceKinds.cpp), read by Vc2OscVoice.cpp and Vc2OscVoiceGet.cpp.  Every address is rom.4's.
// Kinds: 1 LITE, 2 VariPhrase, 3 analog, 4 EXT IN, 5 the VOCODER OSC, 6 the mic's pitch shifter, 7 a second EXT IN-like
// kind whose install writes another render; 0 none; a kind past 7 takes each switch's default.
#include <cstdint>

enum class Vc2OscRender : uint8_t { zero, byRender, byAnalog, carrier, kind6 };   // 0x893ae9f0's ways
enum class Vc2OscRecords : uint8_t { none, fromBlock, atZero };   // the note-on's sample and map: none, +0xd4/+0xdc, 0
enum class Vc2OscPitch : uint8_t { none, bounded, tuned };   // 0x893aeb20: 0; within ±6000; that less +0xa74

struct Vc2OscKindRow {
    uint32_t install = 0;          // 0x893ae0f0: its install, run in its place; 0 none
    uint32_t render = 0;           // the install's +0xa44; 0 none
    int installed = 0;             // the install's +0xa88: its constant's kind (4, 5, 6), 0 none
    bool clearsFlag = false;       // the install's default: +0xa40 to 0
    bool waits = false;            // 0x893ae1c0: the update waits on a pending seek
    uint32_t updateFirst = 0;      // a routine before the ramps' ticks; 0 none
    uint8_t ticks = 0;             // the ramps the update ticks, by bit
    uint32_t update = 0;           // its update after them; 0 none
    Vc2OscRecords records = Vc2OscRecords::none;   // 0x893ae370
    bool flagByWave = false;       // the note-on's flag from the wave's type (+0x104) at 1
    bool ramp = false;             // 0x893ae610's start, and 0x893ae780's set when the block's byte +4 moves
    uint32_t settings = 0;         // 0x893ae780: its settings routine; 0 none
    bool carrier = false;          // the settings' copy into +0x104 and the carrier's settings
    Vc2OscRender renders = Vc2OscRender::byRender;   // 0x893ae9f0: its default, +0xa44
    Vc2OscPitch pitch = Vc2OscPitch::none;           // 0x893aeb20
};

inline constexpr int kVc2OscKinds = 8;   // kinds 0…7 have rows of their own
inline constexpr uint8_t kVc2OscRamp0 = 1, kVc2OscRamp1 = 2, kVc2OscRamp2 = 4;

const Vc2OscKindRow& vc2OscKindRow(uint32_t kind);   // its row; a kind past 7, the defaults' row
