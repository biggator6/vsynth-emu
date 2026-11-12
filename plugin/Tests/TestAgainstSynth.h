#pragma once
// The processor against XtSynth on its own controls, a block at a time: VSynthEmuArpPluginTest's (v190), made one with
// the MSM's checks' (v193).  What the processor gave last, and the most a sample of it has differed from XtSynth's.
#include "TestSynth.h"
#include "../Source/PluginProcessor.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <vector>

struct AgainstSynth {
    AgainstSynth(VSynthEmuProcessor& p, double rate, int block)
        : proc(p), buf(2, block), rl((size_t)block), rr((size_t)block) {
        p.prepareToPlay(rate, block);
        ref = synthOn(rate, p.currentControls());
    }
    // A block: `midi` through processBlock, `notes` played on XtSynth as the processor plays them, both rendered
    void block(juce::MidiBuffer& midi, const std::function<void(XtSynth&)>& notes) {
        buf.clear();
        proc.processBlock(buf, midi);
        juce::ScopedNoDenormals ftz;
        notes(*ref);
        ref->render(rl.data(), rr.data(), buf.getNumSamples());
        for (int i = 0; i < buf.getNumSamples(); ++i) {
            const float l = std::fabs(buf.getSample(0, i) - rl[(size_t)i]);
            const float r = std::fabs(buf.getSample(1, i) - rr[(size_t)i]);
            most = std::max({ most, l, r });
        }
    }

    VSynthEmuProcessor& proc;
    std::unique_ptr<XtSynth> ref;
    juce::AudioBuffer<float> buf;
    std::vector<float> rl, rr;
    float most = 0.0f;
};
