#pragma once
// The part's voices' constants from rom.4 (v272): the table of the 24 voices' addresses, the word a voice not prepared
// reads as its age, and the routines' pool words, read once by loadVc2 at the start (Vc2RomTables), never after.
// HARDWARE_FIRMWARE.md, "The VC-2 part's voices and their lists".
#include <array>
#include <cstdint>

struct Vc2PartVoicesTables {
    std::array<uint32_t, 24> voices {};   // 0x895354b0: each voice's address, by its number
    uint32_t zeroAge = 0;                 // 0x89535510: the age the take reads of a voice not prepared
    float perBudget = 0;                  // 0x89391370: 10^6, CPU Power Max for TG's units in a budget
    float perOffset = 0;                  // 0x89391378: 10^7, CPU Power Offset for Voice's
    float budget = 0;                     // 0x8939138c: the lists' init's budget, 0.0004
    std::array<float, 3> unread {};       // 0x893914b4, 0x893914a4, 0x893914ac: the static initializer's three
    uint32_t retuneWord = 0;              // 0x8953fb2c: the note voice's retune's word, 0 (v345, the part's)
};

[[nodiscard]] bool vc2LoadPartVoices(const char* path);
bool vc2PartVoicesLoaded();
const Vc2PartVoicesTables& vc2PartVoicesTables();
