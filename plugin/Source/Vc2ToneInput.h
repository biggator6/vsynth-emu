#pragma once
// The VC-2 tone's note-on's input (v229): the part's settings for a note as rom.4 lays them out, 0x700 bytes, by the
// offsets the note-on (0x893b3390) reads.  Its blocks go to the parts as they stand; the note-on writes one word back
// (the fix-up).  HARDWARE_FIRMWARE.md, "The VC-2's tone's note-on".
#include <array>
#include <bit>
#include <cstdint>

struct Vc2ToneInput {
    static constexpr uint32_t kBytes = 0x700;
    // [FW] the offsets the note-on reads
    static constexpr uint32_t kStructure = 0x000;   // into the tail's +0x1b24, and the render member pointer by it
    static constexpr uint32_t kPrevious = 0x004;    // the serial of the tone a legato takes from, −1 none
    static constexpr uint32_t kOscSet = 0x00c;      // what each OSC voice's 0x893ae610 is given
    static constexpr uint32_t kLfos = 0x024, kLfoStride = 0x20, kKeySync = 0x10;   // the LFOs' blocks, key sync at +0x10
    static constexpr uint32_t kEnvelopes = 0x124, kEnvelopeStride = 0x38;            // the envelopes' blocks
    static constexpr std::array<uint32_t, 2> kOscs = { 0x434, 0x53c };             // the OSCs' blocks: kind at +0
    static constexpr uint32_t kLegato = 0x64, kWave = 0xb0;   // in an OSC's block: the legato's switch, its +0xb0
    static constexpr uint32_t kLfoValue = 0x494;    // the float each LFO's start is given (OSC 1's block's +0x60)
    static constexpr uint32_t kMix = 0x644;         // the mix's block, its type at +0
    static constexpr std::array<uint32_t, 2> kCosms = { 0x658, 0x6a0 };            // the COSMs' blocks: type at +0
    static constexpr uint32_t kTva = 0x6e8, kTvaLfoA = 0x6ec, kTvaLfoB = 0x6fc;     // the TVA's block: its envelope, LFOs

    std::array<uint32_t, kBytes / sizeof(uint32_t)> words {};
    uint32_t& at(uint32_t offset) { return words[offset / sizeof(uint32_t)]; }
    uint32_t at(uint32_t offset) const { return words[offset / sizeof(uint32_t)]; }
    const uint32_t* block(uint32_t offset) const { return &words[offset / sizeof(uint32_t)]; }
    float floatAt(uint32_t offset) const { return std::bit_cast<float>(at(offset)); }
};
