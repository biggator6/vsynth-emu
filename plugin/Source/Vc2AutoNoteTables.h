#pragma once
// Auto Note's constants from rom.4 (v226): the bend's and the init's pool words, read once by loadVc2 at the start
// (Vc2PoolTables), never after.  HARDWARE_FIRMWARE.md, "Auto Note and the pitch source".

struct Vc2AutoNoteTables {
    // The bend's (0x893ebd80…0x893ebd88)
    float smoothing = 0;     // 0.1: the smoothed period's step toward the tracked one, a call
    float most = 0;          // 20: the movement held to ±20 samples
    float least = 0;         // −20
    // The init's (0x893ebe8c…0x893ebe98)
    float a440Period = 0;    // 44100/440
    float unreadRate = 0;    // 880/44100
    float c4Period = 0;      // 168.5615, 44100 / 261.63
};

[[nodiscard]] bool vc2LoadAutoNote(const char* path);
bool vc2AutoNoteLoaded();
const Vc2AutoNoteTables& vc2AutoNoteTables();
