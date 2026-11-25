// The VOCODER card's bands (UiVc2Bands.h; v459)
#include "UiVc2Bands.h"
#include "UiLook.h"
#include "UiParts.h"
#include <algorithm>
#include <cmath>

namespace {
const char* const kWaits = "waits for the VC-2 part";
const char* const kOff = "Vocoder Sw is OFF: no bands play";
constexpr int kLines = 3, kBarGap = 1;

// A level's bar height under `top`, 0…1 over kDepth dB
float heightOf(float level, float top) {
    if (!(level > 0.0f) || !(top > 0.0f)) return 0.0f;
    const float db = 20.0f * std::log10(level / top);
    return std::clamp((db + Vc2BandsView::kDepth) / Vc2BandsView::kDepth, 0.0f, 1.0f);
}
} // namespace

void Vc2BandsView::update(const Vc2PartView& v, double seconds) {
    constexpr auto relaxed = std::memory_order_relaxed;
    const bool playing = v.playing.load(relaxed);
    const int bands = playing ? std::clamp((int)v.bands.load(relaxed), 0, Vc2PartView::kMostBands) : 0;
    std::array<float, Vc2PartView::kMostBands> mic {}, out {};
    float loudMic = 0.0f, loudOut = 0.0f;
    for (int b = 0; b < bands; ++b) {
        mic[(size_t)b] = std::fabs(v.micBands[(size_t)b].load(relaxed));
        out[(size_t)b] = std::fabs(v.outBands[(size_t)b].load(relaxed));
        loudMic = std::max(loudMic, mic[(size_t)b]);
        loudOut = std::max(loudOut, out[(size_t)b]);
    }
    const float fall = std::pow(10.0f, -kFall * (float)seconds / 20.0f);
    topMic_ = std::max(loudMic, topMic_ * fall);
    topOut_ = std::max(loudOut, topOut_ * fall);
    bands_ = bands;
    for (int b = 0; b < Vc2PartView::kMostBands; ++b) {
        mic_[(size_t)b] = b < bands ? heightOf(mic[(size_t)b], topMic_) : 0.0f;
        out_[(size_t)b] = b < bands ? heightOf(out[(size_t)b], topOut_) : 0.0f;
    }
    said_ = !playing ? kWaits : bands == 0 ? kOff : "";
    repaint();
}

// Each band a column: its mic bar on the left half, its output's on the right, from the bottom up
void Vc2BandsView::paint(juce::Graphics& g) {
    const auto r = getLocalBounds().reduced(6, 4);
    if (said_.isNotEmpty()) {
        Ui::drawWrapped(g, said_, r, { Look::sans(11.0f), Look::muted }, kLines);
        return;
    }
    const float w = (float)r.getWidth() / (float)bands_, h = (float)r.getHeight();
    for (int b = 0; b < bands_; ++b) {
        const float x = (float)r.getX() + w * (float)b, half = std::max(1.0f, w / 2.0f - kBarGap);
        const float bottom = (float)r.getBottom();
        g.setColour(Look::label);
        g.fillRect(x, bottom - h * mic_[(size_t)b], half, h * mic_[(size_t)b]);
        g.setColour(Look::accent);
        g.fillRect(x + w / 2.0f, bottom - h * out_[(size_t)b], half, h * out_[(size_t)b]);
    }
}
