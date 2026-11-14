// Each kind's DETAILS (UiDetailTables.h; UI step 3): which parameter goes where, as UiOscPanel's and UiCosmPanel's
// tables say what their faces show.  A row is the first instance's ID and the name under its control.
#include "UiDetailTables.h"

namespace Blocks {

// OSC: ANALOG's other knobs, then what both types share; PCM's wave (its key, time offset, KFs, start, loop, tempo and
// Beat Keep), then the same.  Each OSC's Time Trip and Robot are the pad's; its LFO's depths are in SELECTED.
const DetailFace kOscDetails[kOscFaces] = {
    { { { { "anaFat", "Fat" }, { "anaFatKF", "Fat KF" }, { "anaFatLfo", "Fat LFO" }, { "anaFatEnv", "Fat Env" },
          { "anaDetune", "Detune" }, { "anaSubOct", "Sub Octave" }, { "anaSubLevel", "Sub Level" },
          { "pitchKF", "Pitch KF" }, { "randomPitch", "Random" }, { "oscLevelKF", "Level KF" },
          { "waveGain", "Gain" }, { "oscVelCurve", "Velo Curve" } } } },
    { { { { "rootKey", "Key" }, { "oscTimeOffset", "Time Offset" }, { "oscTimeKF", "Time KF" },
          { "oscFormantKF", "Fmt KF" }, { "startOffset", "Start" }, { "loop", "Loop" }, { "loopMode", "Loop Mode" },
          { "loopStart", "Loop Start" }, { "loopEnd", "Loop End" }, { "origTempo", "Tempo" },
          { "beatKeep", "Beat Keep" }, { "randomPitch", "Random" }, { "oscLevelKF", "Level KF" },
          { "waveGain", "Gain" }, { "oscVelCurve", "Velo Curve" } } } },
};

// MOD: its whole section, as it has no rack panel
const DetailFace kModDetails[1] = {
    { { { { "modSwitch", "MOD" }, { "modType", "Type" }, { "modLevel", "Level" }, { "modAttack", "Attack" },
          { "modRelease", "Release" } } } },
};

// A COSM, by its type in Type's order: what its panel's five knobs and box leave (the Dynamic TVF's Own depths besides
// the TVF's; the Compressor, the Limiter and the Speaker Simulator have nothing more)
const DetailFace kCosmDetails[kCosmFaces] = {
    { { { { "ResoEnv", "Reso Env" }, { "ResoLfo", "Reso LFO" } } } },
    { { { { "ResoEnv", "Reso Env" }, { "ResoLfo", "Reso LFO" }, { "CutoffOwn", "Cut Own" },
          { "ResoOwn", "Reso Own" } } } },
    { { { { "Dual1Lfo", "LFO 1" }, { "Dual1Env", "Env 1" }, { "Dual2KF", "KF 2" }, { "Dual2Lfo", "LFO 2" },
          { "Dual2Env", "Env 2" } } } },
    { { { { "TbLfo", "LFO" }, { "TbQFollow", "Q Follow" }, { "TbHpf", "HPF" }, { "TbHpfKF", "HPF KF" },
          { "TbHpfLfo", "HPF LFO" }, { "TbHpfEnv", "HPF Env" } } } },
    { { { { "CombToneLfo", "Tone LFO" } } } },
    { { { { "Sb1Octave", "Octave" }, { "Sb1SubDetune", "Sub" }, { "Sb1DetuneLfo", "Det LFO" },
          { "Sb1DetuneOwn", "Det Own" }, { "Sb1DetuneEnv", "Det Env" }, { "Sb1WidthKF", "Width KF" },
          { "Sb1WidthLfo", "Wid LFO" }, { "Sb1WidthOwn", "Wid Own" }, { "Sb1WidthEnv", "Wid Env" } } } },
    { { { { "Sb2Octave", "Octave" }, { "Sb2DetuneLfo", "Det LFO" }, { "Sb2DetuneOwn", "Det Own" },
          { "Sb2DetuneEnv", "Det Env" }, { "Sb2WidthKF", "Width KF" }, { "Sb2WidthLfo", "Wid LFO" },
          { "Sb2WidthOwn", "Wid Own" }, { "Sb2WidthEnv", "Wid Env" } } } },
    { { { { "ResBalanceLfo", "Bal LFO" }, { "ResBalanceEnv", "Bal Env" } } } },
    { { { { "FsBalanceLfo", "Bal LFO" }, { "FsBalanceEnv", "Bal Env" } } } },
    { { { { "LofiFilterLfo", "Filt LFO" }, { "LofiFilterEnv", "Filt Env" }, { "LofiBalanceLfo", "Bal LFO" },
          { "LofiBalanceEnv", "Bal Env" } } } },
    {},
    {},
    { { { { "WsToneKF", "Tone KF" }, { "WsToneLfo", "Tone LFO" }, { "WsToneEnv", "Tone Env" } } } },
    { { { { "OdToneKF", "Tone KF" }, { "OdToneLfo", "Tone LFO" }, { "OdToneEnv", "Tone Env" } } } },
    { { { { "AmpGainLfo", "Gain LFO" }, { "AmpGainEnv", "Gain Env" } } } },
    {},
};

// AMP: the patch TVA's key follows and where it goes (its velocity is on its envelope's tab, its LFO's depths in
// SELECTED)
const DetailFace kAmpDetails[1] = {
    { { { { "tvaLevelKF", "Level KF" }, { "tvaPanKF", "Pan KF" }, { "outputAssign", "Output" },
          { "outputMode", "Output Mode" } } } },
};

}
