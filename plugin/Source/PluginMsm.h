#pragma once
// The Multi Step Modulator's host parameters (v193, PluginMsm.cpp): the plugin's ID for each of the XT's IDs 0x48d…0x4ee,
// which the processor reads into XtSynth's controls and the editor's MSM tab attaches to.
#include <juce_core/juce_core.h>

// "msmSwitch", "msmKeySync", then a track's "msm1Switch" … "msm1Dest", "msm1Step1" … "msm1Step16" (tracks 1…4); an empty
// string past them
const juce::String& msmParamId(int fwId);
