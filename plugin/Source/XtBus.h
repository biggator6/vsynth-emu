#pragma once
// Where XtSynth's output bus can go instead of straight to its stereo mix: XtFxUnit, the effect chip.  So XtSynth and its harnesses need nothing of the chip.
class XtBusSink {
public:
    virtual ~XtBusSink() = default;
    // Audio thread: `frames` frames of the bus, 8 floats each in the XT's units (XtSynth::frame), and ±1 stereo out.
    virtual void process(const float* frame, float* lr, int frames) = 0;
};
