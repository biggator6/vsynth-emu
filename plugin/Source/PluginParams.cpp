// The parameters' layout: the note values', two places' and velocity curves' texts, then the parts in the order a host
// has always seen them; and OSC k's parameter IDs.  Moved as they stood from PluginProcessor.cpp (v190).
#include "PluginProcessor.h"
#include "PluginParamIds.h"
#include "PluginVc2Params.h"

juce::String VSynthEmuProcessor::oscParamId(int k, const juce::String& id) { return Params::oscId(k, id); }

juce::AudioProcessorValueTreeState::ParameterLayout
VSynthEmuProcessor::createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // An envelope's time or an LFO's rate from 128 on is a note value, as the XT's knobs go on past 127: 3 to 192 clocks of 24 to the quarter
    static const char* const kNotes[17] = { "1/32", "1/16", "1/8T", "1/16.", "1/8", "1/4T", "1/8.", "1/4", "1/2T", "1/4.", "1/2", "1/1T", "1/2.", "1/1", "2/1T", "1/1.", "2/1" };
    const auto noteValue = juce::AudioParameterIntAttributes()
        .withStringFromValueFunction([](int v, int) { return v >= 128 ? juce::String(kNotes[std::min(v - 128, 16)]) : juce::String(v); })
        .withValueFromStringFunction([](const juce::String& t) {
            for (int i = 0; i < 17; ++i) if (t.trim() == kNotes[i]) return 128 + i;
            return t.getIntValue();
        });
    // A wave's own loop and tempo hold more than a box can show
    const auto twoPlaces = juce::AudioParameterFloatAttributes().withStringFromValueFunction([](float v, int) { return juce::String(v, 2); });
    auto curveName = juce::AudioParameterIntAttributes().withStringFromValueFunction(
        [](int v, int) { return v == 0 ? juce::String("OFF") : juce::String(v); });
    // The parts, in the order a host has always seen them (PluginParams*.cpp, v190)
    addOscParams(params, twoPlaces);
    addCosmParams(params, noteValue);
    addToneParams(params, noteValue, curveName);
    addOsc2Params(params, noteValue, twoPlaces, curveName);
    addModParams(params);
    addArpParams(params);   // PluginArp.cpp, v190: appended, so every other keeps its place
    addMsmParams(params);   // PluginMsm.cpp, v193: the same
    addZoneParams(params);  // PluginZones.cpp, v195: the same
    addExtInParams(params); // PluginExtIn.cpp, v206: the same
    vc2FindRomEarly();             // PluginRom.cpp, v451: rom.4 in first, so the VC-2's are rom.4's
    vc2AddHostParameters(params);  // PluginVc2Params.cpp, v451: rom.4's, appended

    return { params.begin(), params.end() };
}

// ── The Matrix's lists (moved as they stood from PluginProcessor.cpp, v455) ─────
juce::StringArray VSynthEmuProcessor::matrixSourceNames() {
    juce::StringArray a { "OFF" };
    for (int n = 1; n < 96; ++n) a.add(n == 32 ? juce::String("(OFF)") : "CC" + juce::String(n).paddedLeft('0', 2));
    a.addArray(juce::StringArray { "BEND", "AFT", "+PAD-X", "+PAD-Y", "PAD-X", "PAD-Y", "TRIP-R", "BEAM-L", "BEAM-R", "KNOB1", "KNOB2", "VELO", "KEYF" });
    return a;
}

juce::StringArray VSynthEmuProcessor::matrixDestNames() {
    juce::StringArray a { "OFF" };
    for (const char* o : { "OSC1", "OSC2" }) {
        for (const char* d : { "PITCH", "TIME/PW", "FORMA/FAT", "LVL", "PENV-ATK", "PENV-DCY", "PENV-REL", "TENV-ATK", "TENV-DCY", "TENV-REL",
                               "FENV-ATK", "FENV-DCY", "FENV-REL", "AENV-ATK", "AENV-DCY", "AENV-REL", "LFO-RATE", "LFO-PCH", "LFO-TM/PW", "LFO-FR/FT", "LFO-LVL" })
            a.add(juce::String(o) + "-" + d);
    }
    for (const char* o : { "CSM1", "CSM2" })
        for (const char* d : { "PRM1", "PRM2", "ENV1-ATK", "ENV1-DCY", "ENV1-REL", "ENV2-ATK", "ENV2-DCY", "ENV2-REL", "LFO-RATE", "LFO-PRM1", "LFO-PRM2" })
            a.add(juce::String(o) + "-" + d);
    a.addArray(juce::StringArray { "TVA-LVL", "TVA-ENV-ATK", "TVA-ENV-DCY", "TVA-ENV-REL", "TVA-LFO-RATE", "TVA-LFO-LVL", "TVA-LFO-PAN", "MFX-SEND", "CHO-SEND", "REV-SEND",
                                   "MFX-PRM1", "MFX-PRM2", "MFX-PRM3", "TVA-PAN" });
    return a;
}

juce::StringArray VSynthEmuProcessor::assignValueNames() {
    juce::StringArray a { "OFF" };
    for (int n = 1; n < 96; ++n) a.add(n == 32 ? juce::String("(OFF)") : "CC" + juce::String(n).paddedLeft('0', 2));
    return a;
}

const char* VSynthEmuProcessor::assignParamId(int i) {
    static const char* const k[kAssigns] = { "assignBeam2L", "assignBeam2R", "assignBeam3L", "assignBeam3R", "assignBeam4L", "assignBeam4R", "assignPadX", "assignPadY", "assignKnob1", "assignKnob2", "assignTripX", "assignTripY" };
    return k[i];
}

const char* VSynthEmuProcessor::assignName(int i) {
    static const char* const k[kAssigns] = { "Beam 2 L", "Beam 2 R", "Beam 3 L", "Beam 3 R", "Beam-L", "Beam-R", "Pad X", "Pad Y", "Knob 1", "Knob 2", "Trip X", "Trip Y" };
    return k[i];
}

int VSynthEmuProcessor::assignDefault(int i) { return i == 6 ? 80 : i == 7 ? 81 : i == 10 ? 82 : i == 11 ? 83 : 0; }
