// A loaded wave as the editor draws it (v200; PluginProcessor.h's WaveView): each of kViewColumns columns' lowest and
// highest sample, the channels mixed as the classifier mixes them.  Made once, from the channels encodeWave copies as the
// wave goes to its encode, so every way a wave comes (a file, a patch, a session) makes one.  The editor reads it on the
// message thread; the audio thread never sees it.
#include "PluginProcessor.h"
#include <algorithm>

namespace {

// Frame i of the channels, mixed
float mixed(const std::vector<std::vector<float>>& chans, size_t i) {
    float s = 0.0f;
    for (const auto& c : chans) s += c[i];
    return s / (float)chans.size();
}

// The lowest and highest of frames [from, to)
VSynthEmuProcessor::WaveView::Column span(const std::vector<std::vector<float>>& chans, size_t from, size_t to) {
    VSynthEmuProcessor::WaveView::Column c { mixed(chans, from), mixed(chans, from) };
    for (size_t i = from + 1; i < to; ++i) {
        const float s = mixed(chans, i);
        c = { std::min(c.low, s), std::max(c.high, s) };
    }
    return c;
}

} // namespace

void VSynthEmuProcessor::publishWaveView(int osc, const std::vector<std::vector<float>>& chans) {
    const size_t frames = chans.empty() ? 0 : chans[0].size();
    const size_t columns = std::min<size_t>(WaveView::kViewColumns, frames);
    auto view = std::make_shared<WaveView>();
    for (size_t col = 0; col < columns; ++col) {
        const size_t from = col * frames / columns;
        view->columns.push_back(span(chans, from, std::max(from + 1, (col + 1) * frames / columns)));
    }
    const juce::ScopedLock lock(viewLock_);
    waveView_[osc] = std::move(view);
    ++waveViewGen_[osc];
}

std::shared_ptr<const VSynthEmuProcessor::WaveView> VSynthEmuProcessor::waveView(int osc) const {
    const juce::ScopedLock lock(viewLock_);
    return waveView_[osc];
}
