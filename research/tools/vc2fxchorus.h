#pragma once
// The Chorus on both images (research/VC2_VOCAL_DESIGNER.md, "The Chorus's sides"): what vc2fxchorus.cpp, its
// responses, and vc2fxchorusphase.cpp, its LFO's phase, share.
#include "vc2fxdriver.h"
#include <memory>
#include <vector>

namespace vc2 {

inline constexpr unsigned kChorusSend = 0x26;   // [FW] the frame's Chorus send (vc2fxroute.cpp)
inline constexpr double kTapShare = 0.05;       // a tap: at least this share of the word's largest

// Each out word's response (the Chorus's 0xf0…0xf6, the mix's 0xa0, 0xa2) to an impulse into the Chorus send, less its
// trace with none, on a settled copy of `chip`
using Responses = std::vector<std::vector<double>>;
Responses responsesOf(const XtFxChip& chip);
// Each of rom.4's out words against each of rom.2's: the correlation, and how many samples are equal
void sayAgainst(const Responses& rom4, const Responses& rom2);

// The LFO on both images: each side's delay through its cycle, the lag that lines rom.4's up with rom.2's (its sides as
// they are and crossed), then the same to the sample, with the whole response there
void sayPhase(const XtFxChip& rom2, const XtFxChip& rom4);

// One image's Chorus as the reload test drives it: its chip, run on in silence, and a setting by the image's own ID
// changed and the effects reloaded
struct ChorusImage {
    virtual ~ChorusImage() = default;
    virtual const char* name() const = 0;
    virtual const XtFxChip& chip() const = 0;
    virtual void runOn(int samples) = 0;
    virtual bool set(int rom2Id, int rom4Id, int value) = 0;   // false on a fault
};
// rom.4's, on its driver as hookFxDriver set it up over `chip`, answering from `answers`
std::unique_ptr<ChorusImage> rom4Chorus(Machine& m, XtFxChip& chip, FxSettings& answers);
// rom.2's, on XtFxUnit
std::unique_ptr<ChorusImage> rom2Chorus(std::unique_ptr<XtFxUnit> unit);
// Whether a reload sets the LFO's phase: the LFO's track, then the chip run on, a setting changed and reloaded and set
// back, and the track again: how far on the LFO stands against the run.  False on a fault
bool sayReloads(ChorusImage& c);

} // namespace vc2
