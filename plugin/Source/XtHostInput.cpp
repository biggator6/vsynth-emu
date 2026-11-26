// The plugin's outside input brought to 44.1 kHz for the EXT IN OSCs: see XtHostInput.h (v207).
#include "XtHostInput.h"

#include <algorithm>
#include <climits>
#include <cmath>

namespace {
constexpr int kBlockFrames = 32;              // what XtSynth::busOut takes a block
constexpr double kChipRate = 44100.0;         // [FW] the XT's rate, the chip's and its converters'
constexpr double kFullScale = 2147483648.0;   // ±1 to the int32 the chip's input slots take the top 24 bits of (XtFxInput.cpp)
constexpr int kRoomLeft = 64;                 // frames of out_ a push leaves unfilled, past the latency
}

void XtHostInput::prepare(double hostRate) {
    const double rate = hostRate > 0 ? hostRate : kChipRate;
    step_ = rate / kChipRate;
    const double perHost = kChipRate / rate;   // 44.1 kHz frames a host frame: XtSynth::render's step
    direct_ = std::abs(perHost - 1.0) < 1e-12;   // as render decides it
    sinc_.make(0.5 * std::min(1.0, kChipRate / rate) * 0.95);   // the host's rate → 44.1 kHz, in cycles per host frame
    // What render takes runs ahead of what it has given: up to a block, and with its kernel kHalf frames more; what this
    // makes runs kHalf host frames behind what it was given.  So a block in hand, and those two more (and 2 for the
    // positions' rounding), and no block comes short whatever the host's blocks (VSynthEmuExtInTest)
    latency_ = direct_ ? kBlockFrames : kBlockFrames + kHalf + (int)std::ceil(kHalf * perHost) + 2;
    most_ = std::max(1, (int)(((double)XtExtInput::kFrames - latency_ - kRoomLeft) / perHost));
    history_.fill(0.0f);
    base_ = 0;
    held_ = kHalf;   // the silence before the host's first frame, as render's history starts
    next_ = 0;
    out_.clear();
    takenFrom_ = out_.taken();
    const std::array<int32_t, 2 * kBlockFrames> silence {};
    for (int left = latency_; left > 0; left -= kBlockFrames) out_.push(silence.data(), std::min(left, kBlockFrames));
    short_ = over_ = 0;
}

int32_t XtHostInput::wordOf(float x) {
    if (!std::isfinite(x)) return 0;
    const double v = (double)x * kFullScale;
    if (v >= kFullScale - 1.0) return INT32_MAX;
    if (v <= -kFullScale) return INT32_MIN;
    return (int32_t)v;
}

void XtHostInput::push(const float* l, const float* r, int n) {
    for (int done = 0; done < n;) {
        const int take = std::min(n - done, kHistory - held_);
        for (int i = 0; i < take; ++i) {
            history_[(size_t)(2 * (held_ + i))] = l ? l[done + i] : 0.0f;
            history_[(size_t)(2 * (held_ + i) + 1)] = r ? r[done + i] : 0.0f;
        }
        held_ += take;
        done += take;
        produce();
    }
}

// Frame j sits at kHalf + j × step_ in the history's frames (its first kHalf the silence before the host's first), and
// reads the taps around it (XtSinc::at), or the frame itself at 44.1 kHz
long long XtHostInput::firstTap(long long frame) const {
    if (direct_) return kHalf + frame;
    return (long long)std::floor((double)kHalf + (double)frame * step_) - kHalf + 1;
}

void XtHostInput::produce() {
    std::array<int32_t, 2 * kBlockFrames> made {};
    int n = 0;
    const auto flush = [&] {
        if (out_.held() + (uint32_t)n > XtExtInput::kFrames) ++over_;
        out_.push(made.data(), n);
        n = 0;
    };
    const long long end = base_ + held_;
    for (;; ++next_) {
        const long long first = firstTap(next_), last = first + (direct_ ? 0 : 2 * kHalf - 1);
        if (last >= end) break;
        const long at = (long)(first - base_);
        float l = 0.0f, r = 0.0f;
        if (direct_) {
            l = history_[(size_t)(2 * at)];
            r = history_[(size_t)(2 * at + 1)];
        } else {
            const double x = (double)kHalf + (double)next_ * step_;
            sinc_.at(history_.data(), at + kHalf - 1, x - std::floor(x), l, r);
        }
        made[(size_t)(2 * n)] = wordOf(l);
        made[(size_t)(2 * n + 1)] = wordOf(r);
        if (++n == kBlockFrames) flush();
    }
    if (n > 0) flush();
    // what no tap of the frames to come reads
    const int drop = (int)std::min<long long>(held_, firstTap(next_) - base_);
    if (drop <= 0) return;
    std::copy(history_.begin() + 2 * drop, history_.begin() + 2 * held_, history_.begin());
    base_ += drop;
    held_ -= drop;
}

const int32_t* XtHostInput::block() {
    if (out_.held() < (uint32_t)kBlockFrames) ++short_;
    return out_.block();
}

int XtHostInput::drop(int frames) {
    int dropped = 0;
    while (dropped < frames && out_.held() > 0) {
        const int n = std::min({ frames - dropped, kBlockFrames, (int)out_.held() });
        out_.take(n);
        dropped += n;
    }
    return dropped;
}
