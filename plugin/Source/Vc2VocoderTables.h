#pragma once
// The VC-2's vocoder object's constants from rom.4 (v220), and the edit page's words' first values: read once by
// loadVc2 at the start (Vc2RomTables), never after.  HARDWARE_FIRMWARE.md, "The vocoder object, read".
#include <cstdint>

// The edit page's words, as rom.4 lays them out in .data from 0x8994025c: what the settings path takes in place of the
// settings struct's while the flag is set (0x893f0960 sets it, 0x893f0970 a side's three)
struct Vc2VocoderEdit {
    float ratio[2];     // 0x8994025c: each side's Formant Shift ratio
    int32_t pivot[2];   // 0x89940264: each side's pivot, for HIRESO
    float offset[2];    // 0x8994026c: each side's ratio offset by the tracked pitch
    int32_t on;         // 0x89940274: the flag
};
static_assert(sizeof(Vc2VocoderEdit) == 0x1c, "rom.4's .data, 0x8994025c…0x89940278");

struct Vc2VocoderTables {
    float buildGain = 0;    // the builder's consonants' gain × the HIRESO flag, 0.3 (0x893f0630)
    float gainScale = 0;    // the block drivers' consonants' gain × it, 2^31 (0x893f063c)
    float glide = 0;        // the glue's glide of the gain a block, 0.3 (0x893f08ec)
    float minRatio = 0;     // the glue's lowest ratio, 0.5 (0x893f08f8); its highest is its 2, an immediate
    float periodA440 = 0;   // A440's period in samples at 44.1 kHz, 100.227, the tracker's ratio's (0x893f09f8)
    Vc2VocoderEdit edit {}; // the edit page's words as .data starts them
};

[[nodiscard]] bool vc2LoadVocoder(const char* path);
bool vc2VocoderLoaded();
const Vc2VocoderTables& vc2VocoderTables();
