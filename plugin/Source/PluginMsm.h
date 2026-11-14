#pragma once
// The Multi Step Modulator's host parameters (v193, PluginMsm.cpp): the plugin's ID for each of the XT's IDs 0x48d…0x4ee,
// which the processor reads into XtSynth's controls and the editor's MSM tab attaches to.
#include <juce_core/juce_core.h>

// "msmSwitch", "msmKeySync", then a track's "msm1Switch" … "msm1Dest", "msm1Step1" … "msm1Step16" (tracks 1…4); an empty
// string past them
const juce::String& msmParamId(int fwId);
// The XT's ID of track t's setting i (tracks 0…3): 0…7 its Switch … Destination, 8 on its steps (both editors' tracks;
// the MSM tab's from v193, D's MODULATION page from UI step 4)
int msmTrackFwId(int t, int i);
