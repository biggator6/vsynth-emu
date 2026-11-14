#pragma once
// A wave rom.4's store holds against the plugin's own reading of the stream it came from, for vc2wavein.cpp
// (research/VC2_VOCAL_DESIGNER.md, "The wave reader, run").  The store's answers for an id: chunk 0x89351ad0(id, kind),
// band 0x89351ff0(id, k), frames 0x89351a10(id).  The plugin's: xtWaveRead (XtWaveFile.cpp), its RDAC audio by rom.2's
// block decoder, which feeds zeros past the frames (xtFwRdacDecode); the same decoder over whole blocks (the frames
// rounded up to 16, from the data chunk's own bytes); and, for a BACKING wave, the bands xtMbBuildBacking builds from
// that audio (XtMultiBandPlayer.h: band 0 the full-rate residual, which replaces the audio in sample memory).
#include "vc2runs.h"
#include <array>
#include <cstdint>

namespace vc2 {

inline constexpr uint32_t kChunkKinds = 11, kStoreBands = 7;   // [FW] the fetch asks kinds 2…5 and 10, bands 0, 2…6

// What the samples gave, over the bank
struct WaveTally {
    int unread = 0;          // not held, not found, or not read
    int fieldsDiffer = 0;    // the header's, the loop record's, the vpno's, the vpdt's or the frames
    // band 0 against xtWaveRead's samples: equal, differing only in the last block of 16 frames, or before it
    int asRead = 0, lastBlockOnly = 0, before = 0, partBlocks = 0;
    int wholeDiffer = 0;     // band 0 against whole blocks decoded
    int bandWaves = 0, bandsDiffer = 0;   // BACKING samples, their bands against xtMbBuildBacking's
    std::array<long, kChunkKinds> chunks{};
    std::array<long, kStoreBands> bands{};
    bool failed() const { return unread || fieldsDiffer || wholeDiffer || bandsDiffer; }
};

// One sample: its store's answers against the plugin's reading of `stream`, the differences said and tallied; false
// when the stream isn't read
bool compareStoredWave(Machine& m, uint32_t id, const uint8_t* stream, size_t size, WaveTally& t);
void sayTally(const WaveTally& t, int samples);

} // namespace vc2
