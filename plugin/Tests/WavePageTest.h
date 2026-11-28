#pragma once
// What the WAVE page's checks share (EditorWavePageTest.cpp, v479; EditorWaveCutTest.cpp, v480; EditorWaveZoomTest.cpp,
// v484)
#include "EditorTest.h"
#include "TestControls.h"
#include "TestWave.h"
#include "../Source/PluginParamIds.h"
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

inline bool saysNow(VSynthEmuProcessor& p, const char* message) {
    return p.getStatusText().contains(message);
}

// The processor's status come to say `message`, waited for (EditorWaveEventTest.cpp's, v481; shared by v485)
inline bool comesToSay(VSynthEmuProcessor& p, const char* message) {
    return waitFor([&] { return saysNow(p, message); });
}

// XtSynth's own loop end from the controls the processor plays from (XtSynth::waveLoop's reading of the OSC's Loop
// Start and End: a share of the last frame, rounded)
inline long played(float share, long frames) {
    return std::lround((double)std::clamp(share, 0.0f, 1.0f) * (double)(frames - 1));
}

// The WAVE page shown with a test wave of `seconds` loaded into OSC 1, a PCM OSC, OSC 1 shown; none without the page
inline WavePage* pageWithWave(SynthEditor& ed, VSynthEmuProcessor& p, const juce::File& folder, int seconds,
                              TestChecks& check) {
    auto* page = shownPage<WavePage>(ed, Blocks::kWavePage, check);
    if (page == nullptr) return nullptr;
    setParam(p, Params::kOscType, 1.0f);
    p.loadFile(writeTestWave(folder, seconds), 0);
    page->showOsc(0);
    const long frames = page->view(0).frames();
    check(frames == seconds * 44100L, "the page's wave has " + std::to_string(frames) + " frames");
    return page;
}

// A drag of `p` from its handle to the x of `frame`, released there
inline void dragTo(WaveEditView& v, WaveEditView::Point p, long frame) {
    const auto from = v.handle(p);
    LayoutCheck::dragFrom(v, from, { v.xOf(frame), from.y });
}

} // namespace WavePageTest
