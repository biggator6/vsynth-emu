#pragma once
// The XT's 44.1 kHz out at the host's rate (v455): the blocks a part mixes, held in order and read by XtSinc at the host's
// rate, or frame for frame at 44.1 kHz.  XtSynth's render's, moved here as it stood (XtSynthOut.cpp) so that the VC-2's
// part plays through the same (Vc2PartPlayer).  That part may make more blocks than the read asked for, in a wait inside a
// pass (HARDWARE_FIRMWARE.md, "The VC-2 in the plugin"); those wait here ahead of the host, in the room it was given, and
// the next reads hear them first.  Not the firmware's: the XT runs at 44.1 kHz and has no rate to convert.
#include "XtSinc.h"
#include <cmath>
#include <cstddef>
#include <vector>

class XtRateOut {
public:
    // Not on the audio thread: the host's rate, and `room`, the frames it may hold made and not yet heard (0: as many as
    // are made, the buffer growing as XtSynth's always has; else that many and no more, all of it made now)
    void prepare(double hostRate, size_t room = 0);
    // Audio thread: `n` host frames into `left` and `right`; `more()` whenever the read needs a frame not yet made, which
    // appends frames (append), or silence stands for them
    template <class More>
    void render(float* left, float* right, int n, More&& more) {
        const bool direct = std::abs(step_ - 1.0) < 1e-12;
        for (int j = 0; j < n; ++j) {
            for (;;) {   // the need again after each: an append may move what's held down
                const size_t need = (size_t)std::floor(pos_) + (direct ? 1 : (size_t)kHalf + 1);
                if (frames_ >= need) break;
                const size_t had = frames_;
                more();
                if (frames_ == had) silence(need);
            }
            if (direct) {
                const size_t i = (size_t)pos_;
                left[j] = src_[2 * i];
                right[j] = src_[2 * i + 1];
                pos_ += 1.0;
            } else {
                const long i0 = (long)std::floor(pos_);
                sinc_.at(src_.data(), i0, pos_ - (double)i0, left[j], right[j]);
                pos_ += step_;
            }
        }
        heard();
    }
    // Room for `frames` more at the end, interleaved L/R, for the caller to write; nullptr when the room it was given
    // can't take them
    float* append(int frames);
    // The frames made and not yet heard, in 44.1 kHz frames
    double ahead() const { return (double)frames_ - pos_; }

private:
    static constexpr int kHalf = XtSinc::kHalf;
    static constexpr long kKeep = 4096;   // frames heard past which what's left moves down
    std::vector<float> src_;               // interleaved L/R, kHalf frames of silent history first
    size_t frames_ = 0, room_ = 0;         // frames made; the most it may hold (0 none)
    double pos_ = 0, step_ = 1;            // the read, in source frames; 44100 / the host's rate
    XtSinc sinc_;                          // polyphase windowed sinc, 44.1 kHz → host rate
    void silence(size_t need);             // zeros up to `need`
    void heard();                          // the frames no read needs dropped, past kKeep of them
    void moveDown(long keep);
};
