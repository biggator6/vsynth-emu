// The parameters MIDI edits (PluginMidiEdits.h, v462): kept on the audio thread, read ahead of the parameter, handed to
// it on the message thread.
#include "PluginMidiEdits.h"
#include "PluginGesture.h"
#include <algorithm>
#include <cmath>
#include <limits>

PluginMidiEdits::PluginMidiEdits(juce::AudioProcessorValueTreeState& state) {
    for (auto* p : state.processor.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*>(p)) {
            params_.push_back(r);
            raws_.push_back(state.getRawParameterValue(r->paramID));
        }
    for (int i = 0; i < count(); ++i) byRaw_.emplace_back(raws_[(size_t)i], i);
    std::sort(byRaw_.begin(), byRaw_.end());
    pending_ = std::make_unique<Pending[]>((size_t)count());
    lastSet_.assign((size_t)count(), std::numeric_limits<float>::quiet_NaN());
    startTimerHz(30);
}

PluginMidiEdits::~PluginMidiEdits() { stopTimer(); }

int PluginMidiEdits::indexOf(const juce::String& id) const {
    for (int i = 0; i < count(); ++i)
        if (params_[(size_t)i]->paramID == id) return i;
    return -1;
}

juce::RangedAudioParameter* PluginMidiEdits::param(int index) const {
    return index >= 0 && index < count() ? params_[(size_t)index] : nullptr;
}

void PluginMidiEdits::edit(int index, float value) {
    if (index < 0 || index >= count()) return;
    Pending& p = pending_[(size_t)index];
    const auto& range = params_[(size_t)index]->getNormalisableRange();
    if (p.gen.load(std::memory_order_relaxed) == p.flushed.load(std::memory_order_acquire))
        p.before.store(raws_[(size_t)index]->load(std::memory_order_relaxed), std::memory_order_relaxed);
    p.value.store(range.snapToLegalValue(value), std::memory_order_relaxed);
    p.gen.fetch_add(1, std::memory_order_release);
    edits_.fetch_add(1, std::memory_order_release);
}

float PluginMidiEdits::read(const std::atomic<float>* raw) const {
    if (edits_.load(std::memory_order_acquire) == settled_.load(std::memory_order_acquire)) return raw->load();
    const auto at = std::lower_bound(byRaw_.begin(), byRaw_.end(), std::make_pair(raw, -1));
    if (at == byRaw_.end() || at->first != raw) return raw->load();
    const Pending& p = pending_[(size_t)at->second];
    const uint32_t gen = p.gen.load(std::memory_order_acquire);
    return gen != p.flushed.load(std::memory_order_acquire) ? p.value.load(std::memory_order_relaxed) : raw->load();
}

bool PluginMidiEdits::settle(bool set, int& count) {
    bool open = false;
    for (size_t i = 0; i < params_.size(); ++i) {
        Pending& p = pending_[i];
        const uint32_t gen = p.gen.load(std::memory_order_acquire);
        if (gen == p.flushed.load(std::memory_order_relaxed)) continue;
        const float value = p.value.load(std::memory_order_relaxed), now = raws_[i]->load();
        // the parameter as the run of edits found it, or as this last left it: nobody else has moved it since
        const bool ours = now == p.before.load(std::memory_order_relaxed) || now == lastSet_[i];
        if (set && ours) {
            setAsGesture(*params_[i], value);
            lastSet_[i] = raws_[i]->load();
            ++count;
        }
        p.flushed.store(gen, std::memory_order_release);
        open = open || p.gen.load(std::memory_order_acquire) != gen;
    }
    return !open;
}

int PluginMidiEdits::flush() {
    const std::lock_guard<std::mutex> one(settling_);
    const uint32_t seen = edits_.load(std::memory_order_acquire);
    int count = 0;
    if (seen != settled_.load(std::memory_order_relaxed) && settle(true, count))
        settled_.store(seen, std::memory_order_release);
    return count;
}

void PluginMidiEdits::drop() {
    const std::lock_guard<std::mutex> one(settling_);
    const uint32_t seen = edits_.load(std::memory_order_acquire);
    int none = 0;
    if (settle(false, none)) settled_.store(seen, std::memory_order_release);
}
