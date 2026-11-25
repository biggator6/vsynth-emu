#pragma once
// The VOCODER card's display of the vocoder engine's bands as they play (v459; research/UI_DESIGN_VC2.md, d-14), read
// from the part's view (Vc2PartView) on the editor's timer: LEGACY's 10 or HIRESO's 32 bands across, each a pair of
// bars, the mic's envelope in the band (in `label`) and the band's output (in `accent`), each in dB under the loudest
// band of its own kind as it was held (falling kFall dB a second; the two are on scales of their own), kDepth dB deep.
// While no part has played, or with the vocoder off, a line saying so instead.
#include "Vc2PartView.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

class Vc2BandsView : public juce::Component {
public:
    static constexpr float kDepth = 48.0f, kFall = 24.0f;   // dB the bars span; dB a second the top falls
    // The editor's timer: the view as it stands, `seconds` since the last update
    void update(const Vc2PartView& v, double seconds);
    void paint(juce::Graphics& g) override;
    // For the checks: the bands drawn (0 while a line is), a band's two bars' heights (0…1), the line
    int bands() const { return bands_; }
    float micHeight(int band) const { return mic_[(size_t)band]; }
    float outHeight(int band) const { return out_[(size_t)band]; }
    const juce::String& said() const { return said_; }

private:
    int bands_ = 0;
    std::array<float, Vc2PartView::kMostBands> mic_ {}, out_ {};   // each bar's height, 0…1
    float topMic_ = 0.0f, topOut_ = 0.0f;                          // the loudest of each, as held
    juce::String said_ = "waits for the VC-2 part";
};
