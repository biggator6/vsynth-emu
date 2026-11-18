#pragma once
// rom.4's COSM blocks read into XtCosm's (v262, moved out of Vc2CosmAdapters.cpp as they stood, that file at its line
// limit): a converter's block, or the copy of it an object holds (Vc2CosmAdapter::blockIn), as each type's XtCosm block.
// The tone generator's thread.
#include "Vc2CosmNoteOn.h"
#include "XtCosm.h"

namespace Vc2CosmAdapter {
XtOdBlock odBlockOf(const Vc2CosmBlockWords& b);
XtWsBlock wsBlockOf(const Vc2CosmBlockWords& b);
XtAmpBlock ampBlockOf(const Vc2CosmBlockWords& b);
XtResBlock resBlockOf(const Vc2CosmBlockWords& b);
XtLofiBlock lofiBlockOf(const Vc2CosmBlockWords& b);
XtDualBlock dualBlockOf(const Vc2CosmBlockWords& b);
XtTvfBlock tvfBlockOf(const Vc2CosmBlockWords& b);
// The buffered types' (v263; Vc2CosmBufferedAdapters.cpp)
XtCompBlock compBlockOf(const Vc2CosmBlockWords& b);
XtLimBlock limBlockOf(const Vc2CosmBlockWords& b);
XtFsBlock fsBlockOf(const Vc2CosmBlockWords& b);
} // namespace Vc2CosmAdapter
