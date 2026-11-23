#pragma once
// The VC-2's wave index, parsed (v500): rom.4's start-up parse of "D:\VC-2 Wave.bin" (0x8934d120), which fills the
// wave sets (Vc2WaveSets, v399) that PCM's start reads a VC-2 wave by.  The index is a WLF08 file: a header, two version
// strings, then a 16-byte entry a section, each a number (1 the waves, the velocity's set; 2 the maps, the key's; 3 the
// samples, the alias's) and where the section is.  Each of the velocity's and key's records goes into its set's slot
// (0x89327bc0, 0x89327c50: the slot zeroed, the count raised to the record's number, the table's stream the slot) by
// way of its buffer in the reader's words; each sample's four bytes after its path go into the alias's table
// (0x89327ce0).  The file through rom.4's file calls (Vc2Files), in rom.4's order.  Bit for bit: xt_vc2waves.
// HARDWARE_FIRMWARE.md, "The VC-2's wave index, parsed".  The notes' task.
#include "Vc2Files.h"
#include "Vc2WaveSets.h"
#include <array>
#include <cstdint>

// [FW] the index reader's words at 0x8997af30 (.bss): the index's two version strings, 16 characters and a 0 each,
// then the buffers a section's entry is read into, the velocity's 0x210 bytes, the key's 0x310 and an alias's 0x26.  A
// read runs on from its buffer into the next, as rom.4's does; the port's stops at the words' end, where rom.4's would
// go on into the start-up load's state (the check draws no record that long)
struct Vc2WaveIndexWords {
    std::array<uint8_t, 17> waveVersion {}, presetVersion {};
    std::array<uint8_t, 0x210> velocityRead {};
    std::array<uint8_t, 0x310> keyRead {};
    std::array<uint8_t, 0x26> aliasRead {};
};
static_assert(sizeof(Vc2WaveIndexWords) == 0x568, "rom.4's layout, the index reader's words");

// [FW] the loaders' progress, halfwords from 0 to 100 at 0x899344e4 (.data, 0 after C startup): the index's parse's,
// the start-up load's and the link files' maker's
struct Vc2WaveLoadProgress {
    uint16_t parse = 0, startUp = 0, maker = 0;
};
static_assert(sizeof(Vc2WaveLoadProgress) == 6, "rom.4's layout, the loaders' progress");

// [FW] the parse's state, two words its caller gives (0x8934ce70 sets them so): the index's handle, the section's place
struct Vc2WaveIndexState {
    int32_t handle = -1;
    uint32_t sectionAt = 0;
};

inline constexpr int32_t kVc2IndexParsed = 1, kVc2IndexMissing = -60, kVc2IndexNotWlf = -90;   // [FW] the parse's

class Vc2WaveIndex {
public:
    static constexpr uint32_t kWordsAt = 0x8997af30, kProgressAt = 0x899344e4;
    Vc2WaveIndexWords words {};
    Vc2WaveLoadProgress progress {};
    // 0x8934d120: the index read into the sets, its sections in the file's order (a number past 3, or an entry whose
    // second halfword isn't 1, passed over); 1, or none when it won't open or isn't a WLF0 file
    int32_t parse(Vc2WaveIndexState& state, Vc2Files& files, Vc2WaveSets& sets);
};
