#pragma once
// C's envelopes' and LFOs' constants from rom.4 (v282, v283): the note values' ticks, the sustain levels, the times by
// value, the LFOs' rates by value, and the pool words of the envelope's block writer, release and key factor and of the
// LFO's depths, read once by loadVc2 at the start (Vc2RomTables), never after.  HARDWARE_FIRMWARE.md, "The VC-2 part's
// sets' envelopes", "The VC-2 part's sets' LFOs".
#include <array>
#include <cstdint>

struct Vc2SetEnvelopeTables {
    std::array<uint16_t, 128> notes {};    // 0x89542134: a note value's (128 on) ticks
    std::array<uint16_t, 256> levels {};   // 0x89541f34: a sustain level by its byte
    std::array<uint16_t, 128> times {};    // 0x8953fc90: a time by its value, 0…127
    float noteScale = 0;                   // 0x8939a66c: the block writer's note value's scale
    float releaseScale = 0;                // 0x8939a8f4: the release's
    float levelScale = 0;                  // 0x8939a67c: the sustain level's divisor
    float cents = 0;                       // 0x8939ab50: the key's factor's, an octave
    float unit = 0;                        // 0x8939ab58: its 16.16 one
    std::array<float, 128> rates {};       // 0x8953fdb0: an LFO's rate by its value, 0…127 (v283)
    float depthTop = 0;                    // 0x8939b624: an LFO depth's divisor
    float depthScale = 0;                  // 0x8939b628: its square's scale
};

[[nodiscard]] bool vc2LoadSetEnvelopes(const char* path);
bool vc2SetEnvelopesLoaded();
const Vc2SetEnvelopeTables& vc2SetEnvelopeTables();
