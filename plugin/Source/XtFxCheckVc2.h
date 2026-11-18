#pragma once
// What xt_fx <rom.4> vc2's parts share: XtFxCheckVc2.cpp the blocks' IDs, the patch EQ and the run, XtFxCheckVc2Hand.cpp
// rom.4's driver by hand.  XT_FX_MAIN only.
#ifdef XT_FX_MAIN
#include "XtFxCheck.h"

namespace xtfxcheck {
// [FW] rom.4's message: the flags of its second byte, then of its fourth
inline constexpr uint32_t kSystem = 0x20, kAttenuation = 0x10;
inline constexpr uint32_t kPatch = 0x80, kSwitches = 0x40, kMfx = 0x20, kChorus = 0x10, kReverb = 8;
struct IdBlock { int first, last; uint32_t second, fourth; };
// [FW] The check's own list, from rom.4's readers as they were run, so that a row wrong in XtFxImage.cpp shows.  The last
// three rows are the types' own parameters, each type's table run together.
inline constexpr IdBlock kIdBlocks[] = {
    { 0x13a, 0x13a, 0, kSwitches }, { 0x13d, 0x13f, 0, kSwitches }, { 0x1af, 0x1b4, 0, kSwitches },
    { 0x52b, 0x52b, kAttenuation, 0 },
    { 0x40, 0x40, kSystem, 0 }, { 0x43, 0x43, kSystem, 0 }, { 0x78, 0x78, kSystem, 0 },
    { 0x67, 0x68, kSystem, 0 }, { 0x6f, 0x72, kSystem, 0 }, { 0x7f, 0x7f, kSystem, 0 },
    { 0x1b7, 0x1ba, 0, kMfx }, { 0x23c, 0x23e, 0, kChorus }, { 0x270, 0x271, 0, kReverb },
    { 0x56a, 0x7c4, 0, kMfx }, { 0x7ce, 0x816, 0, kChorus }, { 0x819, 0x8ed, 0, kReverb },
};
inline constexpr int kCategory = 0x12c;   // [FW] the switches' reader reads the patch's Category as well

const IdBlock* listedBlock(int id);                                     // the row of kIdBlocks an ID is in, or nullptr
void editOne(XtFxControl& c, int id, int value, bool asPatch = false);  // one parameter, live or as a patch that has it

// How many of the driver's 2048 words the two hold differently: `a` by anything with a word(i)
template <typename Words> int wordsApart(const Words& a, const XtFxChip& b) {
    int d = 0;
    for (unsigned i = 0; i < 2048; i++) d += (a.word(i) & 0x0fffffff) != (b.word(i) & 0x0fffffff);
    return d;
}

void handMessages(const char* rom, int events, uint32_t seed);   // XtFxCheckVc2Hand.cpp
void handOutsideClock(const char* rom);
} // namespace xtfxcheck
#endif
