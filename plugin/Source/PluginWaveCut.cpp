// Truncate on a loaded wave, the XT's own (PluginWaveEdit.h, v480; HARDWARE_FIRMWARE.md {#wave-truncate-read}): the
// kept frames cut on the XT's 44.1 kHz import, a V-Synth wave's own made LITE, the loops as the XT's screen leaves them
#include "PluginProcessor.h"
#include "PluginWaveEdit.h"
#include <algorithm>

namespace {
constexpr double kXtRate = 44100.0;
constexpr int kXtBits = 16;
constexpr float kFull = 32768.0f;
}

void waveCutOnto(const PluginWaveEdit& edit, std::vector<std::vector<float>>& chans, double& rate, int& bits) {
    if (!edit.cut || chans.empty()) return;
    const size_t ch = chans.size() >= 2 ? 2 : 1;
    const std::vector<int16_t> pcm = xtSynthImport(chans, rate, bits);
    const long frames = (long)(pcm.size() / ch);
    if (frames <= 0) return;
    const long from = std::clamp(edit.keepFrom, 0L, frames - 1);
    const long to = std::clamp(edit.keepTo, from, frames - 1);
    std::vector<std::vector<float>> kept(ch, std::vector<float>((size_t)(to - from + 1)));
    for (long i = from; i <= to; ++i)
        for (size_t c = 0; c < ch; ++c) kept[c][(size_t)(i - from)] = (float)pcm[(size_t)i * ch + c] / kFull;
    chans = std::move(kept);
    rate = kXtRate;
    bits = kXtBits;
}

void waveCutOnto(const PluginWaveEdit& edit, XtWaveFile& own, int& ownType, long frames) {
    if (!edit.cut) return;
    if (ownType >= 0) ownType = 0;
    own.encodeType = 0;
    own.events.clear();
    own.vpdt.clear();
    own.vpno.clear();
    own.vpnoCount = -1;
    own.frames = frames;
}

std::pair<long, long> waveLoopAfterCut(long start, long end, long from, long to) {
    const long last = to - from;
    const bool inside = from < start && end < to;
    const long s = inside ? start - from : 0, e = inside ? end - from : last;
    return { std::max(0L, s), std::min(last, e) };
}

void waveEditBackward(PluginWaveEdit& edit, int mode, int start, int end) {
    edit.backward = true;
    edit.bwdMode = mode;
    edit.bwdStart = start;
    edit.bwdEnd = end;
}

void waveEditCut(PluginWaveEdit& edit, long keepFrom, long keepTo) {
    edit.cut = true;
    edit.keepFrom = keepFrom;
    edit.keepTo = keepTo;
}

bool PluginWaveEdits::truncate(VSynthEmuProcessor& p, int osc, long from, long to, int bwdMode) {
    const auto view = p.waveView(osc);
    const long frames = view ? view->frames : 0;
    if (from < 0 || to >= frames || from >= to) return false;
    {
        const juce::ScopedLock lock(p.bufferLock_);
        PluginWaveEdit& e = p.wave_[osc].edit;
        const long base = e.cut ? e.keepFrom : 0;   // a cut of a cut: frames of the file's wave
        waveEditCut(e, base + from, base + to);
        waveEditBackward(e, bwdMode, 0, (int)(to - from));
    }
    p.reencode(osc);
    return true;
}

void PluginWaveEdits::revert(VSynthEmuProcessor& p, int osc) {
    forget(p, osc);
    XtWaveFile own;
    bool isOwn = false;
    {
        const juce::ScopedLock lock(p.bufferLock_);
        own = p.wave_[osc].own;
        isOwn = p.wave_[osc].isOwn;
    }
    if (isOwn) p.waveToParameters(own, osc);
    p.reencode(osc);
}
