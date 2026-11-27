// A loaded wave as the editor draws it (v200; PluginProcessor.h's WaveView): each of kViewColumns columns' lowest and
// highest sample, the channels mixed as the classifier mixes them.  Made from the channels encodeWave copies as the wave
// goes to its encode, so every way a wave comes (a file, a patch, a session) makes one, and again once the encode has run
// with the frames it holds at 44.1 kHz (v479).  The editor reads it on the message thread; the audio thread never sees it.
#include "PluginProcessor.h"
#include <algorithm>
#include <cmath>

namespace {

constexpr double kXtRate = 44100.0;

// Frame i of the channels, mixed
float mixed(const std::vector<std::vector<float>>& chans, size_t i) {
    float s = 0.0f;
    for (const auto& c : chans) s += c[i];
    return s / (float)chans.size();
}

} // namespace

std::vector<PluginWaveView::Column> PluginWaveView::window(long first, long span, int n) const {
    std::vector<Column> out;
    const long have = (long)mono.size();
    if (have == 0 || span <= 0 || n <= 0) return out;
    const double toFile = rate / kXtRate;
    const bool toEnd = first + span >= frames;
    for (int c = 0; c < n; ++c) {
        const long from = first + span * c / n, to = first + span * (c + 1) / n;   // the XT's frames [from, to)
        const long i0 = std::clamp((long)std::floor((double)from * toFile), 0L, have - 1);
        const long past = toEnd && c == n - 1 ? have : (long)std::ceil((double)to * toFile);
        const long i1 = std::clamp(past, i0 + 1, have);
        const auto [lo, hi] = std::minmax_element(mono.begin() + i0, mono.begin() + i1);
        out.push_back({ *lo, *hi });
    }
    return out;
}

void VSynthEmuProcessor::publishWaveView(int osc, const std::vector<std::vector<float>>& chans, double rate,
                                         long frames) {
    const size_t n = chans.empty() ? 0 : chans[0].size();
    auto view = std::make_shared<WaveView>();
    view->mono.resize(n);
    for (size_t i = 0; i < n; ++i) view->mono[i] = mixed(chans, i);
    view->rate = rate > 0.0 ? rate : kXtRate;
    view->frames = frames >= 0 ? frames : (long)std::lround((double)n * kXtRate / view->rate);
    view->columns = view->window(0, view->frames, (int)std::min<size_t>(WaveView::kViewColumns, n));
    const juce::ScopedLock lock(viewLock_);
    waveView_[osc] = std::move(view);
    ++waveViewGen_[osc];
}

std::shared_ptr<const VSynthEmuProcessor::WaveView> VSynthEmuProcessor::waveView(int osc) const {
    const juce::ScopedLock lock(viewLock_);
    return waveView_[osc];
}
