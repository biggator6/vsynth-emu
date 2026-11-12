#pragma once
// What XtSynth's voice keeps across notes, into the firmware's note as it starts (xt_tone, XT_TONE_MAIN only): its phases,
// and fields a waveform does not use.  The firmware's tone comes from a pool, so it starts from the port's.  Moved from
// XtToneCheck.cpp as it stood (v193), for its MSM mode (XtToneMsm.cpp) to share.
#include <vector>
#include "XtSynth.h"
#include "XtToneFirmware.h"

// An analog voice's fields (as xt_analog compares them), port side.
struct VField { const char* name; uint32_t off; float v; };
std::vector<VField> anaFields(const XtAnalogOsc& o);

// `pv` the voice the note takes, `index` its number, `ctl` the note's zone's controls
void xtToneCarry(Firmware& fw, FwNote& n, const XtVoice* pv, int index, const XtControls& ctl);
