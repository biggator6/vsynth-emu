#pragma once
// A VC-2 COSM converter's input as rom.4 lays it out (v234 by what the Growl read; whole from v235, by its builder
// 0x89399b10 and the dispatcher 0x89399920), and the Matrix's sums its converters add to their parameters.
// HARDWARE_FIRMWARE.md, "The VC-2's COSM converters' frame".
#include <cstdint>

struct Vc2CosmInput {
    uint8_t zone;            // +0x00: the builder's zone, which places the 42 bytes below in the store
    uint8_t second;          // +0x01: 0 for COSM 1, else COSM 2: which Matrix reader the converter calls
    uint8_t noteKey;         // +0x02: the note's (its +0xd), which the keyfollows read
    uint8_t noteVelocity;    // +0x03: the note's (its +0xf), which the velocity curves read
    uint8_t unread4[2];      // +0x04, +0x05
    uint8_t on;              // +0x06: the COSM's switch: from here, 42 bytes of the COSM's in the store
    uint8_t type;            // +0x07: 1…16, the dispatcher's switch
    uint8_t curveA;          // +0x08: the first velocity factor's curve and sensitivity
    uint8_t unread9;
    int16_t sensA;           // +0x0a
    uint8_t curveB;          // +0x0c: the second's
    uint8_t unreadD;
    int16_t sensB;           // +0x0e
    int16_t params[16];      // +0x10…+0x2f: the type's parameters in its IDs' order
    float velocity[2];       // +0x30, +0x34: the dispatcher's velocity factors, 0…1
    uint32_t matrix;         // +0x38: the note's Matrix object M, which the readers are given
};
static_assert(sizeof(Vc2CosmInput) == 0x3c, "rom.4's layout, a COSM converter's input");

// The Matrix's sums a COSM's converter adds to its parameters: rom.4's readers, 0x893a2960 (COSM 1) and 0x893a2aa0
// (COSM 2), each given M, bring its COSM's group of sums up to date and return them, an int16 a destination
// (Vc2Matrix.h's Vc2CosmMatrixReader on M's words; a check's own answers)
class Vc2CosmMatrix {
public:
    virtual ~Vc2CosmMatrix() = default;
    virtual const int16_t* sums(bool second, uint32_t matrix) = 0;
};
