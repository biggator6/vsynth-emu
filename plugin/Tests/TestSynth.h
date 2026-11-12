#pragma once
// XtSynth at a rate on these controls: what a test plays directly, or holds the processor's render to.  VSynthEmuArpTest's
// and VSynthEmuArpPluginTest's set-up, made one with the MSM's checks' (v193).
#include "../Source/XtSynth.h"
#include <memory>

inline std::unique_ptr<XtSynth> synthOn(double rate, const XtSynth::Controls& c) {
    auto s = std::make_unique<XtSynth>();
    s->prepare(rate);
    s->setControls(c);
    return s;
}
