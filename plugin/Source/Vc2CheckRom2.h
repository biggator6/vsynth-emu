#pragma once
// The rom.2 beside a check's rom.4 (v320): the path with its last "rom.4" read as "rom.2", the firmware folder holding
// both images; the tables of rom.2's ports a VC-2 check takes come from it (XtCosm's, XtEnvelope's, XtLfo's, XtOsc's).
// xt_vc2cosm's, xt_vc2tone's and xt_vc2libm's.  Check builds only.
#include <string>

inline std::string vc2Rom2Of(const char* rom4) {
    std::string rom2 = rom4;
    const size_t at = rom2.rfind("rom.4");
    if (at != std::string::npos) rom2.replace(at, 5, "rom.2");
    return rom2;
}
