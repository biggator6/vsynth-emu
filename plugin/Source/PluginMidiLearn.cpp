// MIDI learn's map (PluginMidiLearn.h, v463)
#include "PluginMidiLearn.h"

PluginMidiLearn::PluginMidiLearn(PluginMidiEdits& edits) : edits_(edits) {
    for (auto& m : map_) m.store(-1, std::memory_order_relaxed);
}

void PluginMidiLearn::arm(int index) {
    armed_.store(edits_.param(index) ? index : -1, std::memory_order_release);
    changes_.fetch_add(1, std::memory_order_release);
}

void PluginMidiLearn::set(int cc, int index) {
    for (auto& m : map_)
        if (index >= 0 && m.load(std::memory_order_relaxed) == index) m.store(-1, std::memory_order_relaxed);
    map_[(size_t)cc].store(index, std::memory_order_release);
    changes_.fetch_add(1, std::memory_order_release);
}

void PluginMidiLearn::forget(int index) {
    if (const int cc = controllerOf(index); cc >= 0) set(cc, -1);
}

void PluginMidiLearn::forgetAll() {
    for (int cc = 0; cc < kControllers; ++cc) set(cc, -1);
}

int PluginMidiLearn::controllerOf(int index) const {
    for (int cc = 0; cc < kControllers; ++cc)
        if (index >= 0 && map_[(size_t)cc].load(std::memory_order_acquire) == index) return cc;
    return -1;
}

int PluginMidiLearn::parameterOf(int cc) const {
    return cc >= 0 && cc < kControllers ? map_[(size_t)cc].load(std::memory_order_acquire) : -1;
}

std::vector<std::pair<int, int>> PluginMidiLearn::entries() const {
    std::vector<std::pair<int, int>> out;
    for (int cc = 0; cc < kControllers; ++cc)
        if (const int index = parameterOf(cc); index >= 0) out.emplace_back(cc, index);
    return out;
}

juce::String PluginMidiLearn::toText() const {
    juce::StringArray pairs;
    for (const auto& [cc, index] : entries()) pairs.add(juce::String(cc) + "=" + edits_.param(index)->paramID);
    return pairs.joinIntoString(",");
}

void PluginMidiLearn::fromText(const juce::String& text) {
    forgetAll();
    arm(-1);
    for (const auto& pair : juce::StringArray::fromTokens(text, ",", "")) {
        const int cc = pair.upToFirstOccurrenceOf("=", false, false).getIntValue();
        const int index = edits_.indexOf(pair.fromFirstOccurrenceOf("=", false, false));
        if (pair.contains("=") && cc >= 0 && cc < kControllers && index >= 0) set(cc, index);
    }
}

bool PluginMidiLearn::takes(int cc) const {
    return armed_.load(std::memory_order_acquire) >= 0 || parameterOf(cc) >= 0;
}

bool PluginMidiLearn::take(int cc, int value) {
    if (cc < 0 || cc >= kControllers) return false;
    int armed = armed_.load(std::memory_order_acquire);
    if (armed >= 0 && armed_.compare_exchange_strong(armed, -1, std::memory_order_acq_rel)) set(cc, armed);
    const int index = parameterOf(cc);
    auto* param = edits_.param(index);
    if (param == nullptr) return false;
    edits_.edit(index, param->convertFrom0to1((float)value / 127.0f));
    return true;
}
