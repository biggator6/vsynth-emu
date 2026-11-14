#pragma once
// rom.4's effect driver in the interpreter, on an XtFxChip (research/VC2_VOCAL_DESIGNER.md, "Program A's routing"), as
// XtFxControl runs rom.2's: its bus primitives hooked into the chip with the bytes rom.2's send, each word at once (its
// waits answered at once), and its parameter reads answered from a map over rom.4's defaults.  And the chip's side of a
// measurement, on either image's chip: silence in, a settled copy, words traced after an impulse; and rom.2's effects set
// up through XtFxUnit, to set beside them.  vc2fxdriver.cpp.
#include "XtFxChip.h"
#include "XtFxControl.h"
#include "vc2runs.h"
#include <initializer_list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace vc2 {

// Parameter values by rom.4's ID, over its descriptors' defaults (the store less the centre, as its reads give them)
using FxSettings = std::map<int, int>;

// The hooks into `chip`, with the reads answered from `s` as it stands at each read, each ID read added to `heard`; and
// the kernel's stubs and the null page two MFX types read, as XtFxControl has them
void hookFxDriver(Machine& m, XtFxChip& chip, const FxSettings& s, std::set<int>& heard);
// The start XtFxControl::boot makes of rom.2's: its static constructor, its two starts, then the system's and a patch's
// message.  False on a fault
bool startFxDriver(Machine& m);
// The system's and a patch's message: every block loaded again, with what the settings say now.  False on a fault
bool reloadFx(Machine& m);
// A message of the driver's own, its four bytes as the rest of the firmware sets them (rom.2's: second 0x20 system, 0x10
// Attenuation; fourth 0x80 patch, 0x40 switches, 0x20 MFX, 0x10 Chorus, 0x08 Reverb), taken by its reader.  False on a
// fault
bool fxMessage(Machine& m, uint32_t bytes);
// A Reverb Type's name from rom.4's list ("00:Off" … "14:Garage"), or "(past the list)"
std::string reverbTypeName(Machine& m, int type);

// [FW] GRAM's words written from outside the chip: the SH-4's frame (0x20…0x2e), the converters (0x18, 0x1a) and two
// between them, and the two "Program A's routing" finds read from outside (0x11, 0x13)
inline constexpr unsigned kOutsideWords[] = {0x11, 0x13, 0x18, 0x1a, 0x1c, 0x1e, 0x20,
                                             0x22, 0x24, 0x26, 0x28, 0x2a, 0x2c, 0x2e};
inline constexpr int32_t kImpulse = 1 << 20;
// Each of kOutsideWords set to 0, as before a sample with nothing coming in
void silence(XtFxChip& chip);
// `samples` samples of silence on `chip` itself
void runOn(XtFxChip& chip, int samples);
// A copy of `chip` after half a second of silence
std::unique_ptr<XtFxChip> settled(const XtFxChip& chip);
// GRAM words as written each sample (each read one on, after the sample), for `frames` samples after an impulse of
// `height` (0: none, the baseline) into `in`, on a copy of `chip`; one vector a word
std::vector<std::vector<int32_t>> tracesOf(const XtFxChip& chip, unsigned in, std::initializer_list<unsigned> words,
                                           int frames, int32_t height = kImpulse);
// rom.2's effects with `s` (by rom.2's IDs) over its defaults, through XtFxUnit, settled; null on a fault, said
std::unique_ptr<XtFxUnit> rom2With(const char* path, const FxSettings& s);
// Each of `s` set on `u` by rom.2's ID, the patch loaded and the chip settled; false on a fault, said
bool loadRom2Fx(XtFxUnit& u, const FxSettings& s);

} // namespace vc2
