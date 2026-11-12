// COSM 1's and COSM 2's parameters: each type's own, the envelopes and the LFO.  Moved as they stood from
// createParameterLayout (v190), which calls them in its order.
#include "PluginProcessor.h"
#include "PluginParamIds.h"

void VSynthEmuProcessor::addCosmParams(ParamList& params, const IntText& noteValue) {
    for (int k = 0; k < 2; ++k) {
        using Params::cosmId;
        const juce::String n = k ? "COSM 2 " : "COSM 1 ";
        params.push_back(std::make_unique<juce::AudioParameterBool>(cosmId(k, "Switch"), n + "Switch", false));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(cosmId(k, "Type"), n + "Type",
            juce::StringArray { "TVF", "Dynamic TVF", "Dual Filter", "TB Filter", "Comb Filter", "1st Side Band", "2nd Side Band", "Resonator", "Frequency Shifter", "Lo-Fi", "Compressor", "Limiter", "Wave Shape", "OD/Dist", "Amp Simulator", "Speaker Simulator" }, 0));
        params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "ResoOwn"), n + "Resonance Own Env Depth", -63, 63, 0));
        params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "CutoffOwn"), n + "Cutoff Own Env Depth", -63, 63, 0));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(cosmId(k, "Filter"), n + "Filter Type",
            juce::StringArray { "LP24", "BP24", "HP24", "NOTCH24", "PEAK24", "LP12", "BP12", "HP12", "NOTCH12", "PEAK12", "LP6", "HP6" }, 5));
        params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "Cutoff"), n + "Cutoff", 0, 127, 127));
        params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "Reso"), n + "Resonance", 0, 127, 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            cosmId(k, "CutoffKF"), n + "Cutoff Keyfollow", juce::NormalisableRange<float>(-200.0f, 200.0f, 10.0f), 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "CutoffEnv"), n + "Cutoff Env Depth", -63, 63, 0));
        params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "CutoffLfo"), n + "Cutoff LFO Depth", -63, 63, 0));
        params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "ResoEnv"), n + "Resonance Env Depth", -63, 63, 0));
        params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "ResoLfo"), n + "Resonance LFO Depth", -63, 63, 0));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(cosmId(k, "DualType"), n + "Dual Filter Type",
            juce::StringArray { "LPF/HPF", "LPF->HPF", "BPF/BPF" }, 0));
        for (int f = 1; f <= 2; ++f) {               // the XT's defaults: Frequency 90 and 80, Keyfollow +100%, Q 108 and 120
            const juce::String id = cosmId(k, "Dual") + juce::String(f), name = n + "Filter " + juce::String(f) + " ";
            params.push_back(std::make_unique<juce::AudioParameterInt>(id + "Freq", name + "Frequency", 0, 127, f == 1 ? 90 : 80));
            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                id + "KF", name + "Frequency Keyfollow", juce::NormalisableRange<float>(-200.0f, 200.0f, 10.0f), 100.0f));
            params.push_back(std::make_unique<juce::AudioParameterInt>(id + "Q", name + "Q", 0, 127, f == 1 ? 108 : 120));
            params.push_back(std::make_unique<juce::AudioParameterInt>(id + "Lfo", name + "LFO Depth", -63, 63, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(id + "Env", name + "Env Depth", -63, 63, 0));
        }
        {                                            // the TB Filter, the XT's defaults
            const juce::String tn = n + "TB ";
            auto kfParam = [&](const char* id, const juce::String& name) {
                params.push_back(std::make_unique<juce::AudioParameterFloat>(
                    cosmId(k, id), tn + name, juce::NormalisableRange<float>(-200.0f, 200.0f, 10.0f), 0.0f));
            };
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "TbCutoff"), tn + "LPF Cutoff", 0, 127, 90));
            kfParam("TbKF", "LPF Cutoff Keyfollow");
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "TbLfo"), tn + "LPF Cutoff LFO Depth", -63, 63, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "TbEnv"), tn + "LPF Cutoff Env Depth", -63, 63, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "TbQ"), tn + "LPF Q", 0, 127, 100));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "TbQFollow"), tn + "LPF Q Follow", -63, 63, 25));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "TbHpf"), tn + "HPF Cutoff", 0, 127, 45));
            kfParam("TbHpfKF", "HPF Cutoff Keyfollow");
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "TbHpfLfo"), tn + "HPF Cutoff LFO Depth", -63, 63, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "TbHpfEnv"), tn + "HPF Cutoff Env Depth", -63, 63, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "TbGain"), tn + "Gain", 0, 127, 127));
        }
        {                                            // the Comb Filter, the XT's defaults (Octave +1)
            const juce::String cn = n + "Comb ";
            params.push_back(std::make_unique<juce::AudioParameterChoice>(cosmId(k, "CombOctave"), cn + "Octave",
                juce::StringArray { "-2", "-1", "0", "+1", "+2" }, 3));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "CombDetune"), cn + "Detune", -100, 100, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "CombDetuneLfo"), cn + "Detune LFO Depth", -63, 63, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "CombDetuneEnv"), cn + "Detune Env Depth", -63, 63, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "CombTone"), cn + "Tone", 0, 127, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "CombToneLfo"), cn + "Tone LFO Depth", -63, 63, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "CombToneEnv"), cn + "Tone Env Depth", -63, 63, 0));
        }
        for (int t = 1; t <= 2; ++t) {               // the Side Band Filters, the XT's defaults (Octave −1, Width 16 and 32)
            const juce::String id = cosmId(k, "Sb") + juce::String(t), sn = n + (t == 1 ? "1st" : "2nd") + " Side Band ";
            auto num = [&](const char* sfx, const char* name, int lo, int hi, int def) {
                params.push_back(std::make_unique<juce::AudioParameterInt>(id + sfx, sn + name, lo, hi, def));
            };
            auto kfParam = [&](const char* sfx, const char* name, float def) {
                params.push_back(std::make_unique<juce::AudioParameterFloat>(
                    id + sfx, sn + name, juce::NormalisableRange<float>(-200.0f, 200.0f, 10.0f), def));
            };
            if (t == 1) num("Hpf", "HPF Cutoff", 0, 127, 48);
            else num("Q", "Q", 0, 127, 64);
            num("Lpf", "LPF Cutoff", 0, 127, 64);
            kfParam("LpfKF", "LPF Cutoff Keyfollow", 100.0f);
            if (t == 1) num("SubDetune", "Sub Delay Line Detune", -100, 100, 0);
            params.push_back(std::make_unique<juce::AudioParameterChoice>(id + "Octave", sn + "Octave Select", juce::StringArray { "-2", "-1", "0" }, 1));
            num("Detune", "Detune", -100, 100, 0);
            num("DetuneLfo", "Detune LFO Depth", -63, 63, 0);
            num("DetuneOwn", "Detune Own Env Depth", -63, 63, 0);
            num("DetuneEnv", "Detune Env Depth", -63, 63, 0);
            num("Width", "Side Band Width", 0, 127, t == 1 ? 16 : 32);
            kfParam("WidthKF", "Side Band Width Keyfollow", 0.0f);
            num("WidthLfo", "Side Band Width LFO Depth", -63, 63, 0);
            num("WidthOwn", "Side Band Width Own Env Depth", -63, 63, 0);
            num("WidthEnv", "Side Band Width Env Depth", -63, 63, 0);
        }
        {                                            // the Resonator, the XT's defaults
            const juce::String rn = n + "Resonator ";
            params.push_back(std::make_unique<juce::AudioParameterChoice>(cosmId(k, "ResType"), rn + "Filter Type", juce::StringArray { "1", "2", "3" }, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "ResSize"), rn + "Size", 0, 127, 64));
            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                cosmId(k, "ResSizeKF"), rn + "Size Keyfollow", juce::NormalisableRange<float>(-200.0f, 200.0f, 10.0f), 0.0f));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "ResSizeLfo"), rn + "Size LFO Depth", -63, 63, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "ResSizeEnv"), rn + "Size Env Depth", -63, 63, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "ResBalance"), rn + "Balance", 0, 100, 50));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "ResBalanceLfo"), rn + "Balance LFO Depth", -63, 63, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "ResBalanceEnv"), rn + "Balance Env Depth", -63, 63, 0));
        }
        {                                            // the Frequency Shifter, the XT's defaults
            const juce::String fn = n + "Frequency Shifter ";
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "FsDepth"), fn + "Effect Depth", 0, 127, 80));
            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                cosmId(k, "FsDepthKF"), fn + "Effect Depth Keyfollow", juce::NormalisableRange<float>(-200.0f, 200.0f, 10.0f), 0.0f));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "FsDepthLfo"), fn + "Effect LFO Depth", -63, 63, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "FsDepthEnv"), fn + "Effect Env Depth", -63, 63, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "FsBalance"), fn + "Dry/Effect Balance", 0, 100, 50));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "FsBalanceLfo"), fn + "Balance LFO Depth", -63, 63, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "FsBalanceEnv"), fn + "Balance Env Depth", -63, 63, 0));
        }
        {                                            // Lo-Fi, the XT's defaults
            const juce::String ln = n + "Lo-Fi ";
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "LofiBit"), ln + "Sample Bit Down", 0, 15, 10));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "LofiRate"), ln + "Sample Rate Down", 0, 5, 4));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "LofiFilter"), ln + "Filter", 0, 127, 64));
            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                cosmId(k, "LofiFilterKF"), ln + "Filter Keyfollow", juce::NormalisableRange<float>(-200.0f, 200.0f, 10.0f), 0.0f));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "LofiFilterLfo"), ln + "Filter LFO Depth", -63, 63, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "LofiFilterEnv"), ln + "Filter Env Depth", -63, 63, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "LofiBalance"), ln + "Dry/Effect Balance", 0, 100, 50));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "LofiBalanceLfo"), ln + "Balance LFO Depth", -63, 63, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "LofiBalanceEnv"), ln + "Balance Env Depth", -63, 63, 0));
        }
        {                                            // the Compressor, the XT's defaults
            const juce::String cn = n + "Compressor ";
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "CompAttack"), cn + "Attack", 0, 127, 64));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "CompSustain"), cn + "Sustain", 0, 127, 64));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "CompLevel"), cn + "Output Level", 0, 127, 115));
        }
        {                                            // the Limiter, the XT's defaults
            const juce::String mn = n + "Limiter ";
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "LimThreshold"), mn + "Threshold Level", 0, 127, 64));
            params.push_back(std::make_unique<juce::AudioParameterChoice>(cosmId(k, "LimRatio"), mn + "Ratio",
                                                                          juce::StringArray { "1.5:1", "2:1", "4:1", "100:1" }, 1));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "LimAttack"), mn + "Attack", 0, 127, 64));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "LimRelease"), mn + "Release", 0, 127, 64));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "LimLevel"), mn + "Output Level", 0, 127, 115));
        }
        {                                            // Wave Shape, the XT's defaults
            const juce::String wn = n + "Wave Shape ";
            params.push_back(std::make_unique<juce::AudioParameterChoice>(cosmId(k, "WsType"), wn + "Type",
                                                                          juce::StringArray { "1", "2", "3", "4", "5", "6" }, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "WsLevel"), wn + "Output Level", 0, 127, 127));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "WsDrive"), wn + "Drive", 0, 127, 127));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "WsDriveLfo"), wn + "Drive LFO Depth", -63, 63, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "WsDriveEnv"), wn + "Drive Env Depth", -63, 63, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "WsTone"), wn + "Tone", 0, 127, 127));
            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                cosmId(k, "WsToneKF"), wn + "Tone Keyfollow", juce::NormalisableRange<float>(-200.0f, 200.0f, 10.0f), 0.0f));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "WsToneLfo"), wn + "Tone LFO Depth", -63, 63, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "WsToneEnv"), wn + "Tone Env Depth", -63, 63, 0));
        }
        {                                            // OD/Dist, the XT's defaults
            const juce::String on = n + "OD/Dist ";
            params.push_back(std::make_unique<juce::AudioParameterChoice>(cosmId(k, "OdType"), on + "Type", juce::StringArray { "OD", "DS" }, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "OdLevel"), on + "Output Level", 0, 127, 127));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "OdDrive"), on + "Drive", 0, 127, 127));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "OdDriveLfo"), on + "Drive LFO Depth", -63, 63, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "OdDriveEnv"), on + "Drive Env Depth", -63, 63, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "OdTone"), on + "Tone", 0, 127, 127));
            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                cosmId(k, "OdToneKF"), on + "Tone Keyfollow", juce::NormalisableRange<float>(-200.0f, 200.0f, 10.0f), 0.0f));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "OdToneLfo"), on + "Tone LFO Depth", -63, 63, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "OdToneEnv"), on + "Tone Env Depth", -63, 63, 0));
        }
        {                                            // the Amp Simulator, the XT's defaults
            const juce::String an = n + "Amp Simulator ";
            params.push_back(std::make_unique<juce::AudioParameterChoice>(cosmId(k, "AmpType"), an + "Amp Type", juce::StringArray { "1", "2", "3" }, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "AmpBass"), an + "Bass", 0, 127, 64));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "AmpMiddle"), an + "Middle", 0, 127, 64));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "AmpTreble"), an + "Treble", 0, 127, 64));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "AmpLevel"), an + "Output Level", 0, 127, 127));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "AmpGain"), an + "Gain", 0, 127, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "AmpGainLfo"), an + "Gain LFO Depth", -63, 63, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "AmpGainEnv"), an + "Gain Env Depth", -63, 63, 0));
        }
        params.push_back(std::make_unique<juce::AudioParameterChoice>(cosmId(k, "SpkType"), n + "Speaker Simulator Speaker Type",
            juce::StringArray { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12" }, 0));
        for (const char* ab : { "A", "B" }) {
            const juce::String id = cosmId(k, "Env") + ab, name = n + "Env " + ab + " ";
            params.push_back(std::make_unique<juce::AudioParameterInt>(id + "Attack", name + "Attack", 0, 144, 0, noteValue));
            params.push_back(std::make_unique<juce::AudioParameterInt>(id + "Decay", name + "Decay", 0, 144, 0, noteValue));
            params.push_back(std::make_unique<juce::AudioParameterInt>(id + "Sustain", name + "Sustain", 0, 127, 127));
            params.push_back(std::make_unique<juce::AudioParameterInt>(id + "Release", name + "Release", 0, 144, 127, noteValue));
            params.push_back(std::make_unique<juce::AudioParameterInt>(id + "VelCurve", name + "Velocity Curve", 0, 7, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(id + "VelSens", name + "Velocity Sens", -63, 63, 32));
            params.push_back(std::make_unique<juce::AudioParameterInt>(id + "AttackSens", name + "Attack Velocity Sens", -63, 63, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(id + "DecaySens", name + "Decay Velocity Sens", -63, 63, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(id + "ReleaseSens", name + "Release Velocity Sens", -63, 63, 0));
            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                id + "TimeKF", name + "Time Keyfollow", juce::NormalisableRange<float>(-200.0f, 200.0f, 10.0f), 0.0f));
        }
        const juce::String ln = k ? "LFO 4 " : "LFO 3 ";
        params.push_back(std::make_unique<juce::AudioParameterChoice>(cosmId(k, "LfoWave"), ln + "Waveform",
            juce::StringArray { "SIN", "TRI", "SAW", "SQR", "RND", "TRP", "S&H", "CHAOS" }, 1));
        params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "LfoRate"), ln + "Rate", 0, 144, 98, noteValue));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(cosmId(k, "LfoOffset"), ln + "Offset",
            juce::StringArray { "-100", "-50", "0", "+50", "+100" }, 2));
        params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "LfoDelay"), ln + "Delay", 0, 127, 0));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(cosmId(k, "LfoFadeMode"), ln + "Fade Mode",
            juce::StringArray { "ON <<", "ON >>", "OFF <<", "OFF >>" }, 0));
        params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "LfoFade"), ln + "Fade", 0, 127, 0));
        params.push_back(std::make_unique<juce::AudioParameterBool>(cosmId(k, "LfoKeySync"), ln + "Key Sync", false));
    }
}
