#pragma once
// The VC-2's HIRESO formant warp (v213): rom.4's slot 4, 0x893f15d0(this, side, ratio), which writes one side's map, for
// each of the 32 synthesis bands the fractional analysis band whose envelope it reads (the side's at the object's
// +0x2924, the other's at +0x2b24).  A warp in Hz that holds the first and last bands' centres and moves the side's
// pivot to pivot × ratio, then a cubic from Hz to a band; between ratios 0.95 and 1.05 a blend from the identity, so 1
// is the identity exactly.  Bit for bit: xt_vc2hireso against slot 4 on rom.4's own HIRESO object in Sh4Interp.  Its law
// was held to the firmware first as research/tools/vc2warplaw.cpp's model (VC2_VOCAL_DESIGNER.md, "HIRESO's formant
// warp, run"); HARDWARE_FIRMWARE.md, "HIRESO's formant warp".  Audio thread.
#include "Vc2HiresoTables.h"
#include <array>

using Vc2HiresoMap = std::array<float, Vc2HiresoTables::kBands>;

// The map for a side whose pivot field holds `pivot` (slot 5's, from the template's record: 12 for template 0), at the
// Formant Shift's ratio
Vc2HiresoMap vc2HiresoMapOf(int pivot, float ratio);
