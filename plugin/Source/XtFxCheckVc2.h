#pragma once
// What xt_fx <rom.4> vc2's parts share: XtFxCheckVc2.cpp the blocks' IDs, the patch EQ and the run, XtFxCheckVc2Hand.cpp
// rom.4's driver by hand.  XT_FX_MAIN only.
#ifdef XT_FX_MAIN
#include "XtFxCheckHand.h"

namespace xtfxcheck {
struct IdBlock { int first, last; uint32_t second, fourth; };
// [FW] The check's own list, from rom.4's readers as they were run, so that a row wrong in XtFxImage.cpp shows.  The last
// three rows are the types' own parameters, each type's table run together.  External In Type moves no message: the EXT
// IN request reads it (v400).
inline constexpr IdBlock kIdBlocks[] = {
    { 0x13a, 0x13a, 0, kSwitches }, { 0x13d, 0x13f, 0, kSwitches }, { 0x1af, 0x1b4, 0, kSwitches },
    { 0x52b, 0x52b, kAttenuation, 0 },
    { 0x40, 0x40, kSystem, 0 }, { 0x43, 0x43, kSystem, 0 }, { 0x78, 0x78, kSystem, 0 },
    { 0x67, 0x68, kSystem, 0 }, { 0x6f, 0x72, kSystem, 0 }, { 0x7f, 0x7f, kSystem, 0 },
    { 0x7a, 0x7a, 0, 0 },
    { 0x1b7, 0x1ba, 0, kMfx }, { 0x23c, 0x23e, 0, kChorus }, { 0x270, 0x271, 0, kReverb },
    { 0x56a, 0x7c4, 0, kMfx }, { 0x7ce, 0x816, 0, kChorus }, { 0x819, 0x8ed, 0, kReverb },
};
inline constexpr int kCategory = 0x12c;   // [FW] the switches' reader reads the patch's Category as well
inline constexpr int kExtInType = 0x7a, kUsbExtInType = 0x7f;   // [FW] rom.4's External In Type, USB External In Type

const IdBlock* listedBlock(int id);                                     // the row of kIdBlocks an ID is in, or nullptr
void editOne(XtFxControl& c, int id, int value, bool asPatch = false);  // one parameter, live or as a patch that has it

void handMessages(const char* rom, int events, uint32_t seed);   // XtFxCheckVc2Hand.cpp
void handOutsideClock(const char* rom);
void handInput(const char* rom);                                  // XtFxCheckVc2Input.cpp, v400
void vc2RoutingLaw(const char* rom, uint32_t seed);
void unitInputVc2(const char* rom, uint32_t seed);
void vc2MatrixThroughQueue(const char* rom);                      // XtFxCheckQueue.cpp, v401
void handControllers(const char* rom, int events, uint32_t seed); // XtFxCheckVc2Sends.cpp, v402
void sendsLaw(const char* rom, uint32_t seed);
} // namespace xtfxcheck
#endif
