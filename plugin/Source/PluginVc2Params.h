#pragma once
// The VC-2's host parameters (v451): the IDs of rom.4 a patch's zones 1 and 2 hold, and the system settings the V-Synth's
// parameters are, appended after every other parameter (the user's decisions, 2026-11-21; research/VC2_MODE_PLAN.md,
// step 4.5).  Each is rom.4's ID by its own name, range, default and text (Vc2Params) when rom.4's tables are in as the
// parameters are made; a stand-in otherwise, so that a host sees the same list with rom.4 or without.  HARDWARE_FIRMWARE.md,
// "rom.4's parameters as the plugin's".  Nothing reads them into a sound yet.
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <vector>

// One: rom.4's ID, the zone (0 or 1) and the COSM (0 or 1) it is in, and its host parameter's ID
struct Vc2HostParam {
    int id, zone, cosm;
    juce::String paramId;
};
// The set, in the host's order: the patch's common, its effects' Types and levels, Beam Type to the arpeggio, then zone
// 1's and zone 2's tone, envelopes, LFOs and COSMs, then the system's
const std::vector<Vc2HostParam>& vc2HostParams();
// Its ID: "vc2_193", zone 2's "vc2z2_2e5", COSM 2's "vc2c2_927", both "vc2z2c2_927"
juce::String vc2HostParamId(int id, int zone, int cosm);
inline const juce::String kVc2ParamPrefix = "vc2";   // every one's ID starts so, and no other parameter's
// A stand-in's range, made without rom.4's tables: every value a descriptor of the set gives but a Start Offset's, so
// that a session's value survives an instance made without rom.4
constexpr int kVc2StandInLo = -32768, kVc2StandInHi = 32767;
// One parameter as made now: rom.4's name and range when its tables are in, else the stand-in's
std::unique_ptr<juce::RangedAudioParameter> vc2HostParameter(const Vc2HostParam& p);
// Every one, appended to the layout's list (createParameterLayout, after the External In's)
void vc2AddHostParameters(std::vector<std::unique_ptr<juce::RangedAudioParameter>>& params);
// rom.4 brought in before the parameters are made, from where an instance finds it, quietly (PluginRom.cpp's)
void vc2FindRomEarly();
