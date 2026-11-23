#pragma once
// The VC-2's DETAILS (v453; research/UI_DESIGN_VC2.md, "SYNTH", its DETAILS and COSM tables): a face for each value of a
// block's Type, rows by rom.4's IDs as UiVc2Kinds.h reads them, as D's UiDetailTables.h are the V-Synth's.  An OSC's faces
// follow OSC Type (ANALOG, PCM, EXTIN, VOCODER), a COSM's its seventeen types (THRU to TB-FILTER); MOD and AMP have one.
#include "UiBlockKinds.h"

namespace Vc2Blocks {

extern const Blocks::DetailFace kVc2OscDetails[4];
extern const Blocks::DetailFace kVc2ModDetails[1];
extern const Blocks::DetailFace kVc2CosmDetails[17];
extern const Blocks::DetailFace kVc2AmpDetails[1];

}
