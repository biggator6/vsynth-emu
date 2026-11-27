// A loaded wave's edits from the WAVE page, as XtSynth plays them (PluginWaveEdit.h, v479)
#include "PluginWaveEdit.h"
#include "PluginProcessor.h"
#include "XtPatch.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace {
constexpr float kPercent = 100.0f;
constexpr int kNearest = 256;   // floats tried each way from the plain share: enough for any wave of 2^24 frames
const juce::String kBackward = "bwd", kCut = "cut";
const std::array<juce::Identifier, 2> kStateKeys { "waveEdit", "waveEdit2" };   // OSC 1's, OSC 2's

// A value as the host's parameter holds it once set, and as the state's raw value reads it back (JUCE keeps the
// denormalised value and hands out its normalised copy denormalised again)
float held(const juce::RangedAudioParameter& p, float v) {
    const float kept = p.convertFrom0to1(p.convertTo0to1(v));
    return p.convertFrom0to1(p.convertTo0to1(kept));
}
}

int waveLoopFrame(float percent, long frames) {
    const long last = frames - 1;
    return (int)std::lround((double)std::clamp(percent / kPercent, 0.0f, 1.0f) * (double)last);
}

float waveLoopPercentFor(const juce::RangedAudioParameter& p, int frame, long frames) {
    const long last = frames - 1;
    if (last <= 0) return 0.0f;
    const float plain = (float)(kPercent * (double)frame / (double)last);
    float up = plain, down = plain, best = plain;
    long off = std::abs((long)waveLoopFrame(held(p, plain), frames) - frame);
    for (int k = 0; k < kNearest && off > 0; ++k) {   // the nearest frame it can give, where it can't give this one
        for (const float c : { up, down })
            if (const long d = std::abs((long)waveLoopFrame(held(p, c), frames) - frame); d < off) {
                off = d;
                best = c;
            }
        up = std::nextafter(up, kPercent + 1.0f);
        down = std::nextafter(down, -1.0f);
    }
    return best;
}

void waveEditOnto(const PluginWaveEdit& edit, XtEncoded& e) {
    if (edit.backward) xtWaveBackwardOnto(edit.bwdMode, edit.bwdStart, edit.bwdEnd, e.frames, e);
}

juce::String waveEditText(const PluginWaveEdit& edit) {
    juce::StringArray words;
    if (edit.backward)
        words.add(kBackward + "=" + juce::String(edit.bwdMode) + "," + juce::String(edit.bwdStart) + "," +
                  juce::String(edit.bwdEnd));
    if (edit.cut) words.add(kCut + "=" + juce::String(edit.keepFrom) + "," + juce::String(edit.keepTo));
    return words.joinIntoString(";");
}

PluginWaveEdit PluginWaveEdits::backward(const VSynthEmuProcessor& p, int osc) {
    const juce::ScopedLock lock(p.bufferLock_);
    const auto& e = p.wave_[osc].encoded;
    if (!e || e->bwdMode < 0) return {};
    return { true, e->bwdMode, e->bwdStart, e->bwdEnd };
}

PluginWaveEdit PluginWaveEdits::edit(const VSynthEmuProcessor& p, int osc) {
    const juce::ScopedLock lock(p.bufferLock_);
    return p.wave_[osc].edit;
}

void PluginWaveEdits::setBackward(VSynthEmuProcessor& p, int osc, const PluginWaveEdit& edit) {
    {
        const juce::ScopedLock lock(p.bufferLock_);
        PluginWaveEdit& kept = p.wave_[osc].edit;   // the backward loop's from `edit`, the cut as it was
        if (edit.backward) waveEditBackward(kept, edit.bwdMode, edit.bwdStart, edit.bwdEnd);
        else kept.backward = false;
    }
    p.reencode(osc);
}

void PluginWaveEdits::forget(VSynthEmuProcessor& p, int osc) {
    const juce::ScopedLock lock(p.bufferLock_);
    p.wave_[osc].edit = {};
}

void PluginWaveEdits::toState(const VSynthEmuProcessor& p, juce::ValueTree& state) {
    for (int osc = 0; osc < 2; ++osc) state.setProperty(kStateKeys[(size_t)osc], waveEditText(edit(p, osc)), nullptr);
}

void PluginWaveEdits::fromState(VSynthEmuProcessor& p, const juce::ValueTree& state) {
    const juce::ScopedLock lock(p.bufferLock_);
    for (int osc = 0; osc < 2; ++osc)
        p.wave_[osc].edit = waveEditFromText(state.getProperty(kStateKeys[(size_t)osc]).toString());
}

PluginWaveEdit waveEditFromText(const juce::String& text) {
    PluginWaveEdit edit;
    for (const auto& word : juce::StringArray::fromTokens(text, ";", "")) {
        const juce::String key = word.upToFirstOccurrenceOf("=", false, false);
        const auto v = juce::StringArray::fromTokens(word.fromFirstOccurrenceOf("=", false, false), ",", "");
        if (key == kBackward && v.size() == 3) {
            waveEditBackward(edit, v[0].getIntValue(), v[1].getIntValue(), v[2].getIntValue());
        } else if (key == kCut && v.size() == 2) {
            waveEditCut(edit, (long)v[0].getLargeIntValue(), (long)v[1].getLargeIntValue());
        }
    }
    return edit;
}
