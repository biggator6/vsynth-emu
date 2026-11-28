// What the editor shows of the synth's modulation (XtSynthView.h, v464), and where its newest note plays (v485)
#include "XtSynthView.h"
#include "XtSynth.h"

int32_t xtWaveFrameOf(const XtOscVoice& s) {
    if (!s.on || s.analog || s.extIn || s.wave == nullptr) return -1;
    switch (xtPcmPlayerOf(s)) {
        case XtPcmPlayer::linear: return s.lin.position();
        case XtPcmPlayer::lite: return (int32_t)s.xfg.walkPosition();   // its walk (+0x110), not the note-on's +0x38
        case XtPcmPlayer::solo: return s.solo.position() >> 4;
        case XtPcmPlayer::bands: return s.mb.position() >> 4;
    }
    return -1;
}

void XtSynthView::take(const XtSynth& s) {
    const XtVoice* newest = nullptr;
    for (const XtVoice& v : s.voice) {
        if (!v.active) continue;
        const bool held = !v.releasing, newestHeld = newest != nullptr && !newest->releasing;
        if (newest == nullptr || (held && !newestHeld) || (held == newestHeld && v.age > newest->age)) newest = &v;
    }
    frame_[0].store(newest != nullptr ? xtWaveFrameOf(*newest) : -1, std::memory_order_relaxed);
    frame_[1].store(newest != nullptr ? xtWaveFrameOf(newest->osc2) : -1, std::memory_order_relaxed);
    sounding_.store(newest != nullptr, std::memory_order_release);
    if (newest == nullptr) return;
    for (int i = 0; i < kXtMatrixSlots; ++i) mx_[i].store(newest->mx[i], std::memory_order_relaxed);
    for (int i = 0; i < kXtMsmDests; ++i) msm_[i].store(newest->msm[i], std::memory_order_relaxed);
}

void XtSynthView::none() {
    for (auto& f : frame_) f.store(-1, std::memory_order_relaxed);
    sounding_.store(false, std::memory_order_release);
}
