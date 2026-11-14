#pragma once
// Each kind's DETAILS (research/UI_DESIGN.md, "The blocks and the lower panel"; UI step 3): a face for each value of its
// Type, each the settings neither its rack panel nor SELECTED shows, as the first instance's IDs.  An OSC's faces follow
// OSC Type (ANALOG, PCM), a COSM's its sixteen types; MOD and AMP have one face.  Blocks' kinds table points at them.
#include "UiBlockKinds.h"

namespace Blocks {

inline constexpr int kOscFaces = 2, kCosmFaces = 16;
extern const DetailFace kOscDetails[kOscFaces];
extern const DetailFace kModDetails[1];
extern const DetailFace kCosmDetails[kCosmFaces];
extern const DetailFace kAmpDetails[1];

}
