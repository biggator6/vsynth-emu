#pragma once
// rom.2's entry points and RAM the wave routines call (see research/HARDWARE_FIRMWARE.md): moved as they stood out of
// XtFirmware.cpp (v481) when its encodes went to XtFwEncode.cpp, so both read them from one place
#include <cstdint>

namespace xtfwaddr {

inline constexpr uint32_t kSoloNew = 0x8951dc40, kSoloEncode = 0x8951e120;
inline constexpr uint32_t kLoadInit = 0x89529e90, kLoad = 0x894a9940;
inline constexpr uint32_t kEncMulti = 0x89516000;                       // BACKING/ENSEMBLE encode driver
inline constexpr uint32_t kEncSolo = 0x89515e28;                        // SOLO encode driver
inline constexpr uint32_t kOsAlloc = 0x895113a0, kOsFree = 0x895113e0;  // sample-memory chunks (OS pointers 0x8987c8f0/8f8)
inline constexpr uint32_t kAfterVpdt = 0x89515b40;                      // the note chunk, after the vpdt is written
inline constexpr uint32_t kEncInit = 0x89516400, kDetect = 0x895164c0;  // encode module init; attack detection (depth)
// the sample block: +0 type, +2 channels, +3 the Encode Depth, +8 frames, +0x20 start, +0x24 end, +0x7c samples
inline constexpr uint32_t kSmp = 0x8987c940;
inline constexpr uint32_t kChunks = 0x8987c97c;   // (ptr, size) per chunk kind 0..8, set by 0x89517320
inline constexpr uint32_t kMarkCount = 0x89ff0cfc, kMarkHead = 0x89ff0d04;
// the Encode screen's event add and delete on the markers (v481): a frame 1 … frames − 1 each, their results raw
inline constexpr uint32_t kEventAdd = 0x89516b20, kEventDelete = 0x89516c80;
// the encode start's (0x89516720) commit of the list before its driver: the deleted unlinked, kMarkCount set, the
// events counted, or −1 for more than the length holds
inline constexpr uint32_t kEventsCommit = 0x89512d00;
inline constexpr uint32_t kSrcSetup = 0x8952dec0, kSrcFeed16 = 0x8952e4d0;  // WAV import converter (16-bit input)

} // namespace xtfwaddr
