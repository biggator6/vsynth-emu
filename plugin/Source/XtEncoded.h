#pragma once
// The wave XtSynth plays as the XT encoded it, and the calls that make one: moved from XtSynth.h as it stood (v189).
#include "XtBus.h"
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "XtAnalog.h"
#include "XtCosm.h"
#include "XtEnvelope.h"
#include "XtKeys.h"
#include "XtLfo.h"
#include "XtMultiBandPlayer.h"
#include "XtOsc.h"
#include "XtSoloVoice.h"
#include "XtLinearPlayer.h"
#include "XtLitePlayer.h"

struct XtEncoded {
    int type = 0;                  // 0 LITE (the samples alone), 1 SOLO, 3 BACKING, 5 ENSEMBLE
    int channels = 1;              // the wave's channels (1 or 2)
    long frames = 0;               // at 44.1 kHz
    std::vector<uint8_t> vpdt;     // multi-band, or SOLO in the XT's layout (xtSoloReadVpdt): the firmware's, or the port's records with no event between
    XtMbBands bands;
    XtMbWave wave;
    XtSoloPlan solo;               // SOLO
    // What a V-Synth wave brings of its own (0 in anything imported): its fine tuning (xtWaveTune), and a backward loop
    // that isn't the forward one, its region in frames and Loop Mode (the loop record's +0x10, +0xc and +0x19).  -1: the same.
    XtWaveTune tune;
    int bwdMode = -1, bwdStart = 0, bwdEnd = 0;
    float loopRate[2] = { 1.0f, 1.0f };   // forward and backward: what Vari off walks by inside the loop (xtWaveLoopRate)
    // The wave's `vpno`: the events Playback Mode STEP and EVENT play between, by the vpdt's numbers, and its count (+4).  A band wave
    // or SOLO wave encoded here has them all in order, as the XT's own encodes do; a V-Synth wave brings its own (xtWaveOntoEncoded).
    // A LITE wave has none, and the XT plays STEP and EVENT on it as RETRIGGER: its converter doesn't read the mode.
    std::vector<uint16_t> vpno;
    int vpnoCount = 0;
    bool stored = false;           // played on the wave's own vpdt as the file has it, not an encode (xtSynthEncode's `stored`)
};

// The user's rom.2 (XtSynthRom.cpp): the tables, once a process, and the firmware for the interpreter.  False if it isn't
// usable, or (`why` says) isn't the image the process already plays from, and then nothing is written.
bool xtSynthSetRom(const std::string& path, std::string* why = nullptr);
bool xtSynthRomReady();

// Encodes planar float channels at `sampleRate`.  type: 1 SOLO, 3 BACKING,
// 5 ENSEMBLE; depth: Encode Depth 0..127 (multi-band attack detection).
// The sample is imported as the XT imports a WAV: to int16 (sources wider
// than 16 bits keep their top 16, as the XT's 24/32-bit readers do), then
// the XT's own converter to 44.1 kHz (xtFwImport) when the rate differs.
// `bits`: the source's bits per sample.  `events`: a V-Synth wave's own events (XtWaveFile::events, 1/16 sample), which the XT's
// SOLO encode takes in place of its detection, so a SOLO wave keeps the events it was encoded with or edited to (V-Producer can
// edit them) and its `vpno` names them; whole samples, on the file's own audio at 44.1 kHz, else the detection's.  A band wave
// keeps the detection: its encode reads more of a marker than the file keeps.  `stored`: a V-Synth wave's own vpdt (XtWaveFile::vpdt),
// which is played as it is, no encode, as the XT plays a wave it loads: a SOLO wave's plan read from it (xtSoloReadVpdt), a band wave's
// players reading it with the bands rebuilt from the audio (xtFwLoadBands).  Taken when its events run from 0 to the end, never back,
// on the file's own audio at 44.1 kHz, and a SOLO plan reads; else the encode above (`events`, then the detection).
std::shared_ptr<XtEncoded> xtSynthEncode(const std::vector<std::vector<float>>& channels, double sampleRate,
                                         int type, int depth, std::string* err = nullptr, int bits = 16,
                                         const std::vector<int32_t>* events = nullptr, const std::vector<uint8_t>* stored = nullptr);
