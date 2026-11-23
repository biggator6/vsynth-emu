#pragma once
// VariPhrase's words (v396): rom.4's 0x8939f2a0, which PCM's converter (0x8939e300) calls for a wave of type 1, 3 or 5
// (+0xa7, PCM's start's).  By the record's Playback Mode (+0x5a, the input's +0x62): STEP the two events from the step
// count's (the input's +0x104) within the events' count less one, EVENT the first and last events or, with the wave's
// table flag (+0xa0), the key's by periods of twelve keys about a centre the systems' byte +0x10204 moves to the key
// (0x8939c850), each read from the wave's events' table (its pointer +0xc4, entries from +12, by 0x89300db0, a
// halfword of two bytes) into +0xe8, +0xec and +0xf0 as halfwords; RETRIGGER and LEGATO, and STEP with under two
// events, the Start Offset held below the length, the length and the Start Offset again as words.  The pitch10s and key
// about that centre (LITE's `vc2OscPitchAbout`); the cents of each against A4 for a type 1 with Vari Sw and Robot Voice
// Sw, else against the wave's root key, held to ±6000; the level (+0x18) by the level curve at the record's Energy;
// the wave's 48 bytes from +0xcc; and the bits of +0xe4…+0xe6.  Bit for bit: xt_vc2sets' eighth part.
// HARDWARE_FIRMWARE.md, "The VC-2's VariPhrase words".  Every address is rom.4's.
#include "Vc2NoteMatrix.h"
#include <cstdint>

// 0x8939f2a0 on the input (0x108 bytes) and the block; the wave's halfwords, the systems' byte and the part's Time
// Trip through `reads`
void vc2OscPcmVari(uint8_t* input, Vc2SetReads& reads, uint32_t* block);
