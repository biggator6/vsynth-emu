#pragma once
// rom.4's effect driver in the interpreter, on an XtFxChip (research/VC2_VOCAL_DESIGNER.md, "Program A's routing"), as
// XtFxControl runs rom.2's: its bus primitives hooked into the chip with the bytes rom.2's send, each word at once (its
// waits answered at once), and its parameter reads answered from a map over rom.4's defaults.  vc2fxdriver.cpp.
#include "XtFxChip.h"
#include "vc2runs.h"
#include <map>
#include <set>

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

} // namespace vc2
