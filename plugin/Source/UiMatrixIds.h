#pragma once
// The Matrix's Controls as D reads them (UI step 4; the routes' from step 3): how many, and each Control's parameters by
// index, as the processor names them (VSynthEmuProcessor::matrixParamId).  The MODULATION page's rows and the lower
// panel's routes share them.
#include "PluginProcessor.h"

namespace Matrix {

inline constexpr int kControls = 8, kDests = 2;   // the XT's Matrix Controls (IDs 0x14d…0x174), two destinations each

inline juce::String sourceId(int control) {
    return VSynthEmuProcessor::matrixParamId(control, "Source");
}
inline juce::String destId(int control, int dest) {
    return VSynthEmuProcessor::matrixParamId(control, ("Dest" + juce::String(dest + 1)).toRawUTF8());
}
inline juce::String sensId(int control, int dest) {
    return VSynthEmuProcessor::matrixParamId(control, ("Sens" + juce::String(dest + 1)).toRawUTF8());
}

}
