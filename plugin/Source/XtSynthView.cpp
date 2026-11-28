// What the editor shows of the synth's modulation (XtSynthView.h, v464)
#include "XtSynthView.h"
#include "XtSynth.h"

void XtSynthView::take(const XtSynth& s) {
    const XtVoice* newest = nullptr;
    for (const XtVoice& v : s.voice) {
        if (!v.active) continue;
        const bool held = !v.releasing, newestHeld = newest != nullptr && !newest->releasing;
        if (newest == nullptr || (held && !newestHeld) || (held == newestHeld && v.age > newest->age)) newest = &v;
    }
    sounding_.store(newest != nullptr, std::memory_order_release);
    if (newest == nullptr) return;
    for (int i = 0; i < kXtMatrixSlots; ++i) mx_[i].store(newest->mx[i], std::memory_order_relaxed);
    for (int i = 0; i < kXtMsmDests; ++i) msm_[i].store(newest->msm[i], std::memory_order_relaxed);
}
