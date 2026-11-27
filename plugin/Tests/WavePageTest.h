#pragma once
// What the WAVE page's checks share (EditorWavePageTest.cpp, v479; EditorWaveCutTest.cpp, v480)
#include "EditorTest.h"
#include "../Source/UiWavePage.h"
#include <chrono>
#include <cmath>
#include <functional>
#include <thread>

namespace WavePageTest {

inline constexpr auto kWait = std::chrono::milliseconds(50);
inline constexpr int kWaits = 600;   // 30 s for an encode

inline bool waitFor(const std::function<bool()>& done) {
    for (int i = 0; i < kWaits && !done(); ++i) std::this_thread::sleep_for(kWait);
    return done();
}

// XtSynth's own loop end from the controls the processor plays from (XtSynth::waveLoop's reading of the OSC's Loop
// Start and End: a share of the last frame, rounded)
inline long played(float share, long frames) {
    return std::lround((double)std::clamp(share, 0.0f, 1.0f) * (double)(frames - 1));
}

// A drag of `p` from its handle to the x of `frame`, released there
inline void dragTo(WaveEditView& v, WaveEditView::Point p, long frame) {
    const auto from = v.handle(p);
    LayoutCheck::dragFrom(v, from, { v.xOf(frame), from.y });
}

} // namespace WavePageTest
