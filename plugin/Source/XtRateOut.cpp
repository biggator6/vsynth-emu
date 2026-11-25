// The XT's 44.1 kHz out at the host's rate: see XtRateOut.h (v455).
#include "XtRateOut.h"
#include <algorithm>
#include <cstring>

namespace {
constexpr double kXtRate = 44100.0;   // [FW] the XT's rate
constexpr size_t kSlack = 64;         // frames past the room and the kernel's taps, so a block's append always fits
}

void XtRateOut::prepare(double hostRate, size_t room) {
    const double rate = hostRate > 0 ? hostRate : kXtRate;
    step_ = kXtRate / rate;
    sinc_.make(0.5 * std::min(1.0, rate / kXtRate) * 0.95);   // 44.1 kHz → host rate (XtSinc.h), in cycles per source sample
    room_ = room;
    const size_t made = room ? room + (size_t)kKeep + 2 * (size_t)kHalf + kSlack : (size_t)kHalf;
    src_.assign(made * 2, 0.0f);   // kHalf frames of silent history, and with a room all of it now
    frames_ = (size_t)kHalf;
    pos_ = (double)kHalf;          // output time 0 = the first frame made: no delay
}

float* XtRateOut::append(int frames) {
    const size_t want = (frames_ + (size_t)frames) * 2;
    if (room_ == 0) {
        if (src_.size() < want) src_.resize(want);
    } else {
        if (ahead() + frames > (double)room_) return nullptr;
        if (src_.size() < want) moveDown((long)std::floor(pos_) - kHalf);
        if (src_.size() < (frames_ + (size_t)frames) * 2) return nullptr;
    }
    float* at = src_.data() + frames_ * 2;
    frames_ += (size_t)frames;
    return at;
}

void XtRateOut::silence(size_t need) {
    if (float* at = append((int)(need - frames_))) std::fill(at, src_.data() + frames_ * 2, 0.0f);
}

// drop consumed frames, keeping the history the kernel needs
void XtRateOut::heard() {
    const long keep = (long)std::floor(pos_) - kHalf;
    if (keep > kKeep) moveDown(keep);
}

void XtRateOut::moveDown(long keep) {
    if (keep <= 0) return;
    std::memmove(src_.data(), src_.data() + keep * 2, (frames_ - (size_t)keep) * 2 * sizeof(float));
    frames_ -= (size_t)keep;
    pos_ -= (double)keep;
}
