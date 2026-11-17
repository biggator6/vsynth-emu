#pragma once
// What xt_vc2cosm's Matrix parts share (v247: the third's, v243 and v245, and the set-up's, the fourth): where M, the
// channel's state and the parts lie in Vc2Fw's RAM and where rom.4's sources look for them, rom.4's sixteen source
// routines, M's words drawn, and the sources' world drawn and written: the channel's state and the parts' bytes.
// XT_VC2COSM_MAIN only.
#ifdef XT_VC2COSM_MAIN
#include "Vc2CosmCheck.h"
#include "Vc2MatrixSources.h"
#include <vector>

// In Vc2Fw's RAM: M, the channel's state, the parts
inline constexpr uint32_t kVc2MatrixAt = 0x8C070000, kVc2ChannelAt = 0x8C071000, kVc2PartsAt = 0x8C072000;
inline constexpr uint32_t kVc2PartStride = 0x40;
// [FW] M's channel and part words; the part table rom.4's part source reads by M's part, a part's byte
inline constexpr uint32_t kVc2MatrixLinks = 0x168, kVc2PartTable = 0x8993d040, kVc2PartByte = 0x24;
// [FW] the sources' routines, and the bend's
inline constexpr std::array<uint32_t, 16> kVc2SourceRoutines { 0x893a1de0, 0x893a1df0, 0x893a1e10, 0x893a1e30,
    0x893a1e60, 0x893a1ef0, 0x893a1f30, 0x893a1f70, 0x893a1fb0, 0x893a1fd0, 0x893a1ff0, 0x893a2000, 0x893a2040,
    0x893a2080, 0x893a2130, 0x893a21f0 };
inline constexpr uint32_t kVc2BendRoutine = 0x893a1e10;
inline constexpr int16_t kVc2UnityDepth = 8192;   // a depth whose term is the value: 8192 × v >> 13

// A byte as MIDI gives it, now and then an edge (0, the bipolar reading's centre and either side, 127) or any
inline uint8_t vc2DrawMidiByte(std::mt19937& rng) {
    constexpr std::array<uint8_t, 6> edges { 0, 1, 63, 64, 65, 127 };
    const uint32_t pick = rng() % 16;
    return (uint8_t)(pick < 12 ? rng() % 128 : pick < 15 ? edges[rng() % edges.size()] : rng());
}

// M's words drawn: its head (velocity, keys), the slots (destinations mostly within the limits' table, one in eight any
// byte, whose limit is the curve's word; depths mostly ±127, some any 16 bits, some the unity depth), the uses (now and
// then none of a group's), dirty bits and terms; the routines rom.4's sources, the bend often
inline void vc2DrawMatrixCore(Vc2MatrixWords& m, std::mt19937& rng) {
    for (uint8_t& h : m.head) h = vc2DrawMidiByte(rng);
    for (uint8_t& s : m.sources) s = vc2DrawMidiByte(rng);
    for (Vc2MatrixSlot& s : m.slots) {
        const uint32_t within = rng() % 5 == 0 ? 0 : rng() % Vc2MatrixTables::kDestinations;
        s.dest = (uint8_t)(rng() % 8 == 0 ? rng() : within);
        const std::array<int16_t, 4> depths { (int16_t)((int)(rng() % 255) - 127), (int16_t)((int)(rng() % 255) - 127),
                                              (int16_t)rng(), kVc2UnityDepth };
        s.depth = depths[rng() % depths.size()];
    }
    for (uint16_t& u : m.uses) u = rng() % 4 == 0 ? 0 : (uint16_t)rng();
    for (uint16_t& d : m.dirty) d = rng() % 3 == 0 ? 0 : (uint16_t)rng();
    for (int16_t& t : m.terms) t = rng() % 6 == 0 ? (int16_t)rng() : (int16_t)((int)(rng() % 255) - 127);
    for (uint32_t& r : m.routines)
        r = rng() % 4 == 0 ? kVc2BendRoutine : kVc2SourceRoutines[rng() % kVc2SourceRoutines.size()];
}

// What the sources read besides M and the assignments: the channel's state and the parts' bytes, by the part table
struct Vc2MatrixWorld {
    explicit Vc2MatrixWorld(int parts) : partBytes((size_t)parts) {}
    Vc2MatrixChannel channel {};
    std::vector<uint8_t> partBytes;

    // The channel: the bend centred, at an edge of `most` (a slot's limit) or any; the aftertouches and controllers as
    // MIDI gives them
    void drawChannel(std::mt19937& rng, int32_t most) {
        const std::array<int32_t, 6> edges { most, most + 1, -most, -most + 1, -most - 1, 0 };
        channel.bend = (uint16_t)(rng() % 2 ? 8192 + edges[rng() % edges.size()] : rng());
        channel.aftertouch = vc2DrawMidiByte(rng);
        for (uint8_t& p : channel.polyAftertouch) p = vc2DrawMidiByte(rng);
        for (uint8_t& c : channel.controllers) c = vc2DrawMidiByte(rng);
    }
    void drawParts(std::mt19937& rng) {
        for (uint8_t& b : partBytes) b = vc2DrawMidiByte(rng);
    }
    // The channel's state at kVc2ChannelAt; each part in the table and its byte
    void write(Sh4& cpu) const {
        cpu.write(kVc2ChannelAt, &channel, sizeof channel);
        for (uint32_t p = 0; p < partBytes.size(); ++p) {
            const uint32_t at = kVc2PartsAt + kVc2PartStride * p;
            cpu.write(kVc2PartTable + 4 * p, &at, 4);
            cpu.write(at + kVc2PartByte, &partBytes[p], 1);
        }
    }
};
#endif
