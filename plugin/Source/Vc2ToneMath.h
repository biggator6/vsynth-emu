#pragma once
// The routines the VC-2's ports share, for the plugin (v320, step 4.3's forty-eighth version, item 10's twenty-fifth
// part): Vc2ToneShared (Vc2VocoderOsc.h) without the interpreter.  The velocity curve (rom.4's 0x893a7fb0) and exp2
// (0x893a8340) are rom.2's ports of the same code, XtOsc's (0x894ebd60, 0x894ec090), on its tables from the rom.2 the
// plugin loads beside rom.4; logf and powf are rom.4's own (Vc2LibMath, v257).  They equal rom.4's over what the part
// gives them: a velocity 0…127, exp2 within ±7 octaves (past them rom.4 reads on beyond its tables).  The checks keep
// rom.4's own routines on both sides (RomShared); Vc2Synth's tones take this.  Bit for bit: xt_vc2libm's second part.
// HARDWARE_FIRMWARE.md, "The VC-2's shared routines for the plugin".  Any thread: it reads only tables set at the start.
#include "Vc2VocoderOsc.h"

class Vc2ToneMath final : public Vc2ToneShared {
public:
    int velocity(int velocity, int sens, int curve) const override;
    float exp2(float x) const override;
    float log(float x) const override;
    float pow(float x, float y) const override;
};
