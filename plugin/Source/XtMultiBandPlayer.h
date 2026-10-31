#pragma once
// The V-Synth XT's multi-band VariPhrase players — BACKING (encode type 3,
// rom.2 0x8950e6e0) and ENSEMBLE (type 5, 0x8950db10) — ported from the
// firmware and checked against it in Ghidra's P-code emulator
// (research/ghidra_scripts/EmuBacking.java).  See XtMultiBandPlayer.cpp.
#include <cstddef>
#include <cstdint>
#include <vector>
#include "XtLoop.h"

// The players' interpolation kernel, recombination-filter coefficients and
// per-band constants are firmware tables.  They are read from the user's own
// rom.2 image (research/ghidra_scripts/unpack_firmware.py), never shipped:
// returns false if the file is missing or does not look right.
bool xtMbLoadTables(const char* romPath);
bool xtMbTablesLoaded();

// One band as the player addresses it: int16 data (interleaved L/R for
// stereo).  Reads are allowed at frame indices [lo, hi) relative to `data`
// and give 0 outside, so a band can be a slice of one sample-memory image.
struct XtMbBand { const int16_t* data = nullptr; long lo = 0, hi = 0; };

// A loaded multi-band wave: its vpdt chunk (read in place) and the six bands
// the player uses — loader slots 0 and 2..6.  BACKING bands are PCM (band 0
// at the original rate, bands 1..5 decimated ×4 … ×64); ENSEMBLE bands 1..5
// are the six-sinusoid frames built at load time.
struct XtMbWave {
    const uint8_t* vpdt = nullptr;
    size_t vpdtBytes = 0;
    int channels = 1;
    XtMbBand band[6];
};

// The band data the XT builds when it loads a BACKING wave (0x894a9940 with
// the filter bank 0x89522af0 / 0x89522e20 / 0x89523050): a ×4 then four ×2
// decimate-and-interpolate stages over the zero-extended wave.  Band 0 is the
// full-rate residual (it replaces the original audio in sample memory),
// bands 1..4 the residuals at ×4 … ×32, band 5 the last low-pass at ×64.
// Each band is stored as the XT lays it out: L = length rounded up to 64
// frames (shifted per band), then 256 frames of padding holding the stage
// input past the end and ten reversed samples from before the start.
struct XtMbBands {
    int channels = 1;
    long len[6] = {};                  // frames of band data (without padding)
    std::vector<int16_t> buf[6];       // interleaved, (len + 256) frames
};
bool xtMbBuildBacking(const int16_t* pcm, long frames, int channels, XtMbBands& out);
// Point a wave's bands at a built set (band k readable over its buffer).
void xtMbAttach(const XtMbBands& b, XtMbWave& w);

// The four cached loop events (forward start, forward end, backward end,
// backward start) of a multi-band wave as its player reads them (player
// +0x6c, 0xb4 bytes apart): {u16 event, u16 mode, u32 position (1/16
// sample)}, then each band's {u32 segment entry, u32 record, u16 mark} in
// the vpdt's event layout.  A band's slot holds the record containing the
// point and the mark to that record's end, as the wave's own events do (the
// encoder's events, rebuilt this way, come out identical); the event is the
// last one before the point (0x8950f9c0).  The firmware's own builder of
// these events is not found.
constexpr int kXtMbLoopEventBytes = 0xb4;
bool xtMbLoopEvents(const XtMbWave& w, const XtLoop& l, uint8_t out[4][kXtMbLoopEventBytes]);

class XtMbPlayer {
public:
    // Note-on (0x89510980 / 0x89510130): mode 3 = BACKING, 5 = ENSEMBLE.
    // pitch = frequency ratio, walk = playback speed (1/stretch; 0 holds the
    // walk position; < 0 plays backward from the end event, the grains read
    // backward and inverted, as the firmware plays them).
    // Returns false if the tables are not loaded or the wave is unusable.
    bool noteOn(const XtMbWave& w, int mode, float pitch, float walk);
    // One 32-frame block: out[2·i] = L, out[2·i + 1] = R, int16 scale.  Runs
    // in round-to-zero like the SH-4 (XT_ROUND=nearest: the emulator's mode).
    void block(float* out);
    bool ended() const;
    // Pitch and walk may change while the note plays (read every block).
    void setRates(float pitch, float walk);
    // The output gain target (+0x80; 1 = unity), followed per sample pair
    // (0x8950c570); now: the gain jumps there too (note-on).
    void setGain(float target, bool now = false);
    // Loop playback (set before noteOn; off by default): the cached loop
    // events (xtMbLoopEvents) and byte 0 bit 0.  Each block finds its next
    // crossing (0x8950ccc0); the walk list acts on it once per block
    // (0x8950c880 / 0x8950ca70, type 0x10: an Alternate bounce turns the
    // walk from the next block on); a loop jump (0x8950f710) restores the
    // bands from the cached event, keeping the walk's overshoot.
    void setLoop(const XtLoop& l) { loopCfg = l; }

    struct Band;   // a band's player state (firmware struct, 0x88 bytes)
    struct State;
private:
    State* s = nullptr;
    XtLoop loopCfg;
    static void blockImpl(State& S, float* out);
public:
    XtMbPlayer();
    ~XtMbPlayer();
    XtMbPlayer(const XtMbPlayer&) = delete;
    XtMbPlayer& operator=(const XtMbPlayer&) = delete;
};
