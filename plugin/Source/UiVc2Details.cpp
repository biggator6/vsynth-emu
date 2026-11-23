// The VC-2's DETAILS (UiVc2Details.h; v453): which parameter goes where, rom.4's IDs for OSC 1 and COSM 1, as the design's
// tables give them (research/UI_DESIGN_VC2.md, "SYNTH").  The dimmed ones (d-6) are dimmed by their IDs (Vc2Blocks::
// unread), not here.
#include "UiVc2Details.h"

namespace Vc2Blocks {

// OSC, by OSC Type: what its face's box and knobs leave; the vibrato under every Type (rom.4 reads it under each), and
// the second Wave Number, Wave start offset and Harm Formant's depths, which nothing found reads, dimmed
const Blocks::DetailFace kVc2OscDetails[4] = {
    { { { { "2f7", "Fat" }, { "2f8", "Fat KF" }, { "2f9", "Fat LFO" }, { "2fa", "Fat Env" }, { "34d", "Detune" },
          { "34e", "Sub Octave" }, { "34f", "Sub Level" }, { "2e8", "Pitch KF" }, { "2ed", "Random" },
          { "2ef", "Level KF" }, { "2e6", "Gain" }, { "37c", "Vibrato" }, { "37d", "Vib Rate" },
          { "37e", "Vib Depth" } } } },
    { { { { "30b", "Time Offset" }, { "30d", "Fmt KF" }, { "306", "Start" }, { "313", "Loop" }, { "311", "Robot" },
          { "2ed", "Random" }, { "2ef", "Level KF" }, { "2e6", "Gain" }, { "37c", "Vibrato" }, { "37d", "Vib Rate" },
          { "37e", "Vib Depth" }, { "37f", "Wave Start" }, { "389", "Wave No. 2" } } } },
    { { { { "2ed", "Random" }, { "2ef", "Level KF" }, { "37c", "Vibrato" }, { "37d", "Vib Rate" },
          { "37e", "Vib Depth" }, { "373", "Harm LFO" }, { "374", "Harm Env" } } } },
    { { { { "357", "Noise HPF" }, { "359", "Dyn Thresh" }, { "35a", "Dyn Sens" }, { "35c", "Growl KF" },
          { "35d", "Growl LFO" }, { "35e", "Growl Env" }, { "360", "Bal LFO" }, { "361", "Bal Env" },
          { "2e8", "Pitch KF" }, { "2ed", "Random" }, { "2ef", "Level KF" }, { "2e6", "Gain" }, { "37c", "Vibrato" },
          { "37d", "Vib Rate" }, { "37e", "Vib Depth" } } } },
};

// MOD: its whole section, as it has no rack panel
const Blocks::DetailFace kVc2ModDetails[1] = {
    { { { { "2d4", "MOD" }, { "2d5", "Type" }, { "2d6", "Level" }, { "2d7", "Attack" }, { "2d8", "Release" } } } },
};

// A COSM, by COSM Type's value: what its panel's box and five knobs leave (the Speaker, the Compressor and the Limiter
// have nothing more); the GROWL's five that do nothing (Vc2CosmGrowl.h), dimmed
const Blocks::DetailFace kVc2CosmDetails[17] = {
    {},
    { { { { "92d", "Tone KF" }, { "92e", "Tone LFO" }, { "92f", "Tone Env" } } } },
    { { { { "936", "Tone KF" }, { "937", "Tone LFO" }, { "938", "Tone Env" } } } },
    { { { { "93f", "Gain LFO" }, { "940", "Gain Env" } } } },
    {},
    { { { { "948", "Bal LFO" }, { "949", "Bal Env" } } } },
    { { { { "94e", "Octave" }, { "94d", "Sub" }, { "950", "Det LFO" }, { "951", "Det Own" }, { "952", "Det Env" },
          { "954", "Width KF" }, { "955", "Wid LFO" }, { "956", "Wid Own" }, { "957", "Wid Env" } } } },
    { { { { "95b", "Octave" }, { "95d", "Det LFO" }, { "95e", "Det Own" }, { "95f", "Det Env" }, { "961", "Width KF" },
          { "962", "Wid LFO" }, { "963", "Wid Own" }, { "964", "Wid Env" } } } },
    { { { { "967", "Det LFO" }, { "968", "Det Env" }, { "969", "Tone" }, { "96a", "Tone LFO" },
          { "96b", "Tone Env" } } } },
    { { { { "970", "LFO 1" }, { "971", "Env 1" }, { "973", "KF 2" }, { "975", "LFO 2" }, { "976", "Env 2" } } } },
    { { { { "97a", "Reso Env" }, { "979", "Reso LFO" } } } },
    { { { { "981", "Reso LFO" }, { "982", "Reso Own" }, { "983", "Reso Env" }, { "987", "Cut Own" } } } },
    {},
    {},
    { { { { "996", "Bal LFO" }, { "997", "Bal Env" } } } },
    { { { { "99c", "Filt LFO" }, { "99d", "Filt Env" }, { "99f", "Bal LFO" }, { "9a0", "Bal Env" } } } },
    { { { { "9a3", "LFO" }, { "9a6", "Q Follow" }, { "9a7", "HPF" }, { "9a8", "HPF KF" }, { "9a9", "HPF LFO" },
          { "9aa", "HPF Env" } } } },
};

// AMP: the TVA's switch, its key follows and where it goes (its velocity is on its envelope's tab, its LFO's depths in
// SELECTED)
const Blocks::DetailFace kVc2AmpDetails[1] = {
    { { { { "2d9", "TVA" }, { "2db", "Level KF" }, { "2de", "Pan KF" }, { "2e3", "Output" } } } },
};

}
