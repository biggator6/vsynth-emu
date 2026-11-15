// XtSynth's output: the bus to the plugin's stereo, through the effects or straight, and the render at the host's rate
// (moved from XtSynth.cpp as they stood, v204; busOut takes what the chip hands back for the EXT IN OSCs).
#include "XtSynth.h"

#include <algorithm>
#include <cmath>
#include <cstring>

// What the plugin plays: through the effects if they're in, or the pairs that reach the outs.  Without them MULTI has no MFX, and the sends no effects, to go to.
// At the XT's level both ways: a full-scale wave, 2^31 in the frame, comes out at ⅛, as it leaves the chip (XtFxUnit), by Out Gain for TG's −24 and the chip's +6 dB.  A power of two moves no bits.
// The outside input's block (v207) goes the way the bus goes, so the two ways are one decision: into the chip's slots
// before it runs, or as the chip would hand it back.  Both reach the EXT IN OSCs the next block, a frame late.
void XtSynth::busOut(float* lr) {
    XtExtInFeed* feed = input.feed();
    const int32_t* outside = feed ? feed->block() : nullptr;
    if (XtBusSink* s = sinkOn ? sink.load(std::memory_order_acquire) : nullptr) {
        if (outside) s->pushInput(outside, 32);
        s->process(frame, lr, 32);
        // what the chip hands back of the input: the EXT IN OSCs' next block (v204; XtExtIn.h, XtBus.h)
        if (const int32_t* back = s->returned()) input.push(back, 32);
        return;
    }
    if (outside) feed->withoutChip(input, outside);
    for (int i = 0; i < 32; ++i) {
        const float* f = frame + kFrameWords * i;
        lr[2 * i] = (f[FrameMultiL] + f[FrameMainL] + f[FrameDirL]) * 0x1p-34f;
        lr[2 * i + 1] = (f[FrameMultiR] + f[FrameMainR] + f[FrameDirR]) * 0x1p-34f;
    }
}

void XtSynth::render(float* left, float* right, int n) {
    pickUpSample();
    constexpr int kHalf = XtSinc::kHalf;
    const bool direct = std::abs(step - 1.0) < 1e-12;
    for (int j = 0; j < n; ++j) {
        const size_t need = (size_t)std::floor(readPos) + (direct ? 1 : (size_t)kHalf + 1);
        while (srcFrames < need) {
            if (src.size() < (srcFrames + 32) * 2) src.resize((srcFrames + 32) * 2);
            mixBlock(src.data() + srcFrames * 2);
            srcFrames += 32;
        }
        if (direct) {
            const size_t i = (size_t)readPos;
            left[j] = src[2 * i];
            right[j] = src[2 * i + 1];
            readPos += 1.0;
        } else {
            const long i0 = (long)std::floor(readPos);
            sinc.at(src.data(), i0, readPos - (double)i0, left[j], right[j]);
            readPos += step;
        }
    }
    hostFrames += n;
    // drop consumed frames, keeping the history the kernel needs
    const long keep = (long)std::floor(readPos) - kHalf;
    if (keep > 4096) {
        std::memmove(src.data(), src.data() + keep * 2, (srcFrames - (size_t)keep) * 2 * sizeof(float));
        srcFrames -= (size_t)keep;
        readPos -= (double)keep;
    }
}
