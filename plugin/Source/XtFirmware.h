#pragma once
// Load-time and encode-time V-Synth XT routines run from the user's own
// rom.2 in the SH-4 interpreter (Sh4Interp): the firmware's code itself,
// bit-for-bit, with nothing of it in this source.  Used where a native port
// does not exist (ENSEMBLE's load-time analysis) and to validate the ports.
#include <cstdint>
#include <map>
#include <vector>
#include "XtMultiBandPlayer.h"
#include "XtSoloEncoder.h"
#include "XtSoloVoice.h"

// rom.2 path (research/ghidra_scripts/unpack_firmware.py).  Returns false if
// the image cannot be read.
bool xtFwOpen(const char* romPath);
bool xtFwOpened();

// The SOLO encoder (0x8951dc40 + 0x8951e120), as XtSoloEncoder.cpp ports it: n frames
// of mono, or of interleaved stereo in the mode the XT's driver gives a stereo wave
// (0x89515ed6: 0x8951e120(3), and (2) for mono).
bool xtFwSoloEncode(const int16_t* x, int n, std::vector<XtRecord>& out, int channels = 1, std::vector<uint64_t>* trace = nullptr);

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
    XtLoop loop;
};
bool xtFwSoloPlay(const std::vector<XtRecord>& recs, const int16_t* pcm, long frames, int channels,
                  const XtFwSoloNote& note, long outFrames, std::vector<float>& out);

// That one-segment SOLO vpdt, and the player's four cached loop events for
// `loop` (18 bytes each, 0xb4 apart at the player's +0x6c), as xtFwSoloPlay
// writes them.
constexpr int kXtFwSoloEventBytes = 18;
void xtFwSoloImage(const std::vector<XtRecord>& recs, long frames, int channels, const XtLoop& loop,
                   std::vector<uint8_t>& vpdt, uint8_t events[4][kXtFwSoloEventBytes]);

// The firmware's own BACKING (3) or ENSEMBLE (5) player, for validation: the
// load-time band rebuild, then the player set up by its note-on (0x89510980
// / 0x89510130, the start or end event by the sign of walk) and run per
// 32-frame block (0x8950e6e0 / 0x8950db10), as research/ghidra_scripts/
// EmuBacking.java runs it.  pitch and the walk changes as for xtFwSoloPlay
// (formant and energy unused); note.loop: the cached loop events as
// xtMbLoopEvents builds them.  out: L/R pairs, ±1.
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
bool xtFwOscOutGains(std::vector<float>& out);   // the same for Out Gain for TG, 2^(g/6), g −48…+48

// The Matrix's maps: each Dest's slot (0x894e7d10) and each Dest's Sens −63…+63 as stored (0x894e82e0),
// from the firmware's own routines: 79 and 79 × 127 values.
bool xtFwMatrixTables(std::vector<uint8_t>& slots, std::vector<int16_t>& sens);

// The Pitch knob's ratios 2^(c/1200) by the firmware's powf, c the depth
// curve's cents (0x894e0ce0, run here too), the exponent formed as the
// VariPhrase converter forms it (+0xc), Pitch = −63…+63 (XtOsc).
bool xtFwOscPitchRatios(std::vector<float>& out);

// The COSM Side Band Filters' LPF keyfollow squares (0x894dc540): the
// firmware's powf(x, 2), x = (key − 60)·KF·10/100/12 as it forms it, for KF
// = −20…+20 and keys 0..127: 41 × 128 values, by KF then key (XtCosm).
bool xtFwCosmKeySquares(std::vector<float>& out);

// A band player's reads, for a check that runs the firmware's players on an Sh4 of its
// own with the port beside them.  The backward repeat (0x8950f50a) can run a position to
// where the XT reads whatever is in its RAM, which is not known here, and the port reads
// 0.  The check lays kXtFwWavePad of zeros around each band, which cover the near reads;
// a far one lands on anything mapped, so the hook gives it those zeros instead.
constexpr uint32_t kXtFwWavePad = 0x40000;
struct XtFwBandReads {
    std::map<uint32_t, uint32_t> bandEnd;   // the waves' bands: start, end
    long far = 0;                           // band reads far from their wave, given zeros
    void add(uint32_t start, size_t bytes) { bandEnd[start] = start + (uint32_t)bytes; }
};
class Sh4;
void xtFwHookBandReads(Sh4& c, XtFwBandReads& reads);

// Round to nearest instead of FPSCR.RM (to match Ghidra's P-code emulator).
void xtFwForceNearest(bool on);
