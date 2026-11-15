#pragma once
// Where XtSynth's output bus can go instead of straight to its stereo mix: XtFxUnit, the effect chip.  So XtSynth and its harnesses need nothing of the chip.
#include <cstdint>

class XtBusSink {
public:
    virtual ~XtBusSink() = default;
    // Audio thread: `frames` frames of the bus, 8 floats each in the XT's units (XtSynth::frame), and ±1 stereo out.
    virtual void process(const float* frame, float* lr, int frames) = 0;
    // Audio thread, after process(): what the chip handed back for the EXT IN OSCs, int32 L then R a frame, words 4 and 5 of
    // each frame the XT's DMA returns (XtExtIn.h); nullptr for a sink that hands nothing back (v204)
    virtual const int32_t* returned() const { return nullptr; }
};
