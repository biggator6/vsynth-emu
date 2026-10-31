#pragma once
// Load-time and encode-time V-Synth XT routines run from the user's own
// rom.2 in the SH-4 interpreter (Sh4Interp): the firmware's code itself,
// bit-for-bit, with nothing of it in this source.  Used where a native port
// does not exist (ENSEMBLE's load-time analysis) and to validate the ports.
#include <cstdint>
#include <vector>
#include "XtMultiBandPlayer.h"
#include "XtSoloEncoder.h"
#include "XtSoloVoice.h"

// rom.2 path (research/ghidra_scripts/unpack_firmware.py).  Returns false if
// the image cannot be read.
bool xtFwOpen(const char* romPath);
bool xtFwOpened();

// The SOLO encoder (0x8951dc40 + 0x8951e120), as XtSoloEncoder.cpp ports it.
bool xtFwSoloEncode(const int16_t* x, int n, std::vector<XtRecord>& out);

// The firmware's own SOLO player, for validation: the records written as the
// XT lays out a one-segment SOLO vpdt (a start and an end event, their
// segment entries, the records and the (256, 1) terminator); note-on through
// the firmware's start (0x8950fa60, normal playback: a seek to the wave's
// start, or its end when walk < 0); the rest of the note-on state as
// 0x8950fba0 leaves it (research/ghidra_scripts/EmuSolo.java); then
// 0x8950cd90 per 32-frame block.  pcm: mono or interleaved stereo int16.
// walkAt: (frame, walk) changes, applied at the first block starting at or
// after the frame.  out: L/R pairs, ±1.
struct XtFwSoloNote {
    float pitch = 1, formant = 1, walk = 1, energy = 0;
    std::vector<std::pair<long, float>> walkAt;
    std::vector<std::pair<long, float>> gainAt;   // output gain targets (1 = unity)
    bool robot = false;                           // Robot Voice (player byte 0 bit 0x10)
    // Loop playback: the player's cached loop events (+0x6c) built for these
    // points, each the record containing its point as an event jump reads
    // it; player byte 0 bit 0 = on.  The firmware's own builder of these
    // events is not found; this is the state its jump (0x8950d7d0) loads.
    XtSoloLoop loop;
};
bool xtFwSoloPlay(const std::vector<XtRecord>& recs, const int16_t* pcm, long frames, int channels,
                  const XtFwSoloNote& note, long outFrames, std::vector<float>& out);

// The firmware's own BACKING (3) or ENSEMBLE (5) player, for validation: the
// load-time band rebuild, then the player set up by its note-on (0x89510980
// / 0x89510130, the start or end event by the sign of walk) and run per
// 32-frame block (0x8950e6e0 / 0x8950db10), as research/ghidra_scripts/
// EmuBacking.java runs it.  pitch and the walk changes as for xtFwSoloPlay
// (formant and energy unused).  out: L/R pairs, ±1.
bool xtFwBandPlay(int type, const int16_t* pcm, long frames, int channels, const std::vector<uint8_t>& vpdt,
                  const XtFwSoloNote& note, long outFrames, std::vector<float>& out);

// The band data the XT builds when it loads a BACKING (3) or ENSEMBLE (5)
// wave (0x89529e90, 0x894a9940).  Each of the six bands the players use
// (loader slots 0, 2..6) is copied with the memory up to the next band, as
// the player sees it.
bool xtFwLoadBands(int type, const int16_t* pcm, long frames, int channels, XtMbBands& out);

// The multi-band encoder for BACKING (3) / ENSEMBLE (5): the encode driver
// 0x89516000 run to completion.  `vpdt` receives the chunk the firmware
// builds (0x89511a20 / 0x89511de0 / 0x89511e90 / 0x89511c10).  Markers:
// depth < 0 gives a start and an end marker (one segment, a sustained note);
// depth 0..127 runs the XT's attack detection at that Encode Depth (module
// init 0x89516400, then 0x895164c0(depth)), as the encode page does.
// `events`, if given, receives the number of events in the vpdt.
bool xtFwEncodeMulti(int type, const int16_t* pcm, long frames, int channels, std::vector<uint8_t>& vpdt,
                     int depth = -1, int* events = nullptr);

// The XT's WAV import converter to 44.1 kHz: setup 0x8952dec0 (step =
// rate·2^24/44100), the 16-bit driver 0x8952e4d0 over the whole sample, then
// its flush.  The resampler (0x8952dc60 mono / 0x8952dd20 stereo) is an
// 8-phase × 16-tap polyphase FIR with linear interpolation between phases,
// clipped to int16.  `pcm` is interleaved int16 at `rate`; `out` receives
// interleaved int16 at 44.1 kHz.
bool xtFwImport(const int16_t* pcm, long frames, int channels, int rate, std::vector<int16_t>& out);

// The envelope Time KF scales (0x894df760) for kf = −200, −190, … 200 and
// keys 0..127: 41 × 128 values, by kf then key.
bool xtFwEnvKeyScales(std::vector<uint32_t>& out);

// The VariPhrase Time KF scales: 2^(kf·(key − 60)/1200) by the firmware's
// powf (0x8952ff9c), the exponent formed as 0x894e0da0 forms it, for the
// same 41 kf × 128 keys (XtOsc).
bool xtFwOscTimeKeyScales(std::vector<float>& out);

// The Wave Gain factors 2^(dB/6) by the firmware's powf, the exponent formed
// as the VariPhrase converter forms it (0x894e3568), dB = −12…+12.
bool xtFwOscWaveGains(std::vector<float>& out);

// The Pitch knob's ratios 2^(c/1200) by the firmware's powf, c the depth
// curve's cents (0x894e0ce0, run here too), the exponent formed as the
// VariPhrase converter forms it (+0xc), Pitch = −63…+63 (XtOsc).
bool xtFwOscPitchRatios(std::vector<float>& out);

// Round to nearest instead of FPSCR.RM (to match Ghidra's P-code emulator).
void xtFwForceNearest(bool on);
