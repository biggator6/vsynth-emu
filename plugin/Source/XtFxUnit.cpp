// The effects whole: the chip, its driver and the frame's way through (XtFxControl.h; moved from XtFxControl.cpp as it
// stood, v274).
#include "XtFxChip.h"
#include "XtFxControl.h"
#include <climits>

// ------------------------------------------------------------------ the unit
XtFxUnit::XtFxUnit(const char* romPath, const XtFxImage& image)
    : chip_(std::make_unique<XtFxChip>()), control_(romPath, nullptr, image), frameGain_(image.frameGain),
      feedsUsb_(image.feedsUsb) {}
XtFxUnit::~XtFxUnit() = default;

void XtFxUnit::settle() {
    XtFxChip& chip = *chip_;
    while (ok() && !control_.drained()) { control_.pump(); control_.service(chip); chip.runFrame(); }
}

int32_t XtFxUnit::toChip(float x) {
    // FTRC: toward zero, and to the limits past them.  This is on the XT's numbers: a frame's gain, a power of two, moves
    // no bits.  (The limits, spelled out, as a cast past them is undefined in C++.  An ARM host's cast happens to do the
    // same, so no check run here can show them missing; an Intel host's doesn't.)
    const int32_t i = x >= 2147483648.0f ? INT32_MAX : x <= -2147483648.0f ? INT32_MIN : (int32_t)x;
    return i >> 8;
}

float XtFxUnit::fromChip(int32_t word) { return (float)word * 0x1p-23f; }

void XtFxUnit::process(const float* frame, float* lr, int frames) {
    XtFxChip& chip = *chip_;
    const int32_t* in = in_.take(frames);                                         // the outside input's frames, zeros past what came
    for (int i = 0; i < frames; ++i) {
        control_.service(chip);
        const float* f = frame + 8 * i;
        for (unsigned w = 0; w < 8; ++w) chip.setGram(0x20 + 2 * w, toChip(f[w] * frameGain_));
        frameIn(in, i);
        chip.runFrame();
        frameBack(i);
        // what a program wrote at a slot reads one on, once the chip has rotated
        lr[2 * i] = fromChip(chip.gram(0xa1)) + fromChip(chip.gram(0xa5));
        lr[2 * i + 1] = fromChip(chip.gram(0xa3)) + fromChip(chip.gram(0xa7));
    }
}
