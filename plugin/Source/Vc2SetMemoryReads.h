#pragma once
// C's reads of memory at rom.4's addresses (v341, Vc2Synth whole's second version): what C, M and C's children read
// past their own words that is the part's memory as it stands, over a memory its deriving class gives, rom.4's
// interpreter's in xt_vc2sets (SetPortParts), the part's own in Vc2Synth.  Moved from SetPortParts (v280…v399) as they
// stood, each at its address: the voice by a call, an object's first byte, K's channel state, a channel's bend and
// controllers, the part's tables, M's controllers and bend, the TVA's globals, the clock object, the OSC table, a
// glide key, a key table's Hold, the legato's bytes, Time Trip's moving word.  The children's routines and the reads
// that run rom.4's routines (a Matrix group's sums, the OSC groups, the pitch source, the velocity curve) are the
// deriving class's.  Bit for bit: xt_vc2sets.  The notes' and update tasks.
#include "Vc2Sets.h"
#include <array>
#include <cstddef>

// [FW] the parts' words for a child, by part, kind and index (an index's the words' count apart): where they start, a
// part's and a kind's stride, how many a kind has; the envelopes' (v282), the LFOs' (v283), the mix's (v284), the
// TVA's (v285), the COSM inputs' 42 bytes (v390)
struct Vc2SetPartTable {
    uint32_t at, part, kind, perKind;
};
inline constexpr Vc2SetPartTable kVc2SetEnvelopeTable { 0x89e2765c, 2080, 130, 13 };
inline constexpr Vc2SetPartTable kVc2SetLfoTable { 0x89e2f85c, 800, 50, 5 };
inline constexpr Vc2SetPartTable kVc2SetMixTable { 0x89e32a60, 352, 22, 1 };
inline constexpr Vc2SetPartTable kVc2SetTvaTable { 0x89e4987c, 384, 24, 1 };
inline constexpr Vc2SetPartTable kVc2SetCosmTable { 0x89e2003c, 0x540, 84, 2 };
// [FW] the part's bytes by kind, its three words the common takes, a channel's controllers, the clock object (v282);
// the TVA's globals (the part's controllers' flag, the part table's trailer) and M's controllers (v285)
inline constexpr uint32_t kVc2SetModesAt = 0x89998a14, kVc2SetCommonsAt = 0x89997090, kVc2SetCommonBytes = 6;
inline constexpr uint32_t kVc2SetControls = 0x84, kVc2SetClockAt = 0x8995cb40;
inline constexpr uint32_t kVc2SetTvaFlagAt = 0x89997fcc, kVc2SetTrailerAt = 0x89e4b07c, kVc2SetMatrixControls = 0x178;
// [FW] the parts' OSC table (v391): a part's row, its header, a kind's and a record's bytes, the systems after 16 rows;
// a key table's Hold, M's bend; the part table and a trip object's moving word (v395)
inline constexpr uint32_t kVc2OscTableAt = 0x89e371e0, kVc2OscPartRow = 0x1020, kVc2OscHeaderBytes = 32;
inline constexpr uint32_t kVc2OscKindRow = 0x100, kVc2OscRecordBytes = 0x80, kVc2OscTableParts = 16;
inline constexpr uint32_t kVc2OscSystemsAt = kVc2OscTableAt + kVc2OscTableParts * kVc2OscPartRow;
inline constexpr uint32_t kVc2OscHoldAt = 0x11c, kVc2OscBendAt = 0x174;
inline constexpr uint32_t kVc2OscPartTableAt = 0x8993d040, kVc2OscTripMovingAt = 8;
// [FW] the OSCs' step counts (v343: the part's reads write one): 64 bytes a part, 4 a kind, 2 an OSC
inline constexpr uint32_t kVc2OscStepsAt = 0x89989c84, kVc2OscStepsPart = 64;

class Vc2SetMemoryReads : public Vc2SetParts {
public:
    Vc2SetVoice voiceAt(uint32_t channelAt) override;
    uint8_t firstByte(uint32_t object) override;
    uint32_t channelOf(uint32_t keys) override;
    int16_t bendOf(uint32_t channel) override;
    uint8_t controlOf(uint32_t channel, uint32_t number) override;
    uint8_t modeOf(uint32_t part, uint32_t kind) override;
    std::array<uint16_t, 3> commonOf(uint32_t part) override;
    std::array<uint16_t, 5> envelopeOf(uint32_t part, uint32_t kind, uint32_t index) override;
    std::array<uint16_t, 5> lfoOf(uint32_t part, uint32_t kind, uint32_t index) override;
    std::array<uint16_t, 11> mixOf(uint32_t part, uint32_t kind) override;
    std::array<uint16_t, 12> tvaOf(uint32_t part, uint32_t kind) override;
    uint8_t matrixControl(uint32_t matrix, uint32_t n) override;
    Vc2SetTvaGlobals tvaGlobals() override;
    Vc2SetClock clock() override;
    std::array<uint16_t, 21> cosmInputOf(uint32_t part, uint32_t kind, uint32_t index) override;
    std::array<uint8_t, 32> oscHeader(uint32_t part) override;
    std::array<uint8_t, 128> oscRecord(uint32_t part, uint32_t kind, uint32_t osc) override;
    std::array<uint8_t, 60> oscSystems() override;
    Vc2SetGlide glideAt(uint32_t glide) override;
    uint8_t holdOf(uint32_t keys) override;
    std::array<uint8_t, 2> legatoOf(uint32_t word) override;
    int32_t matrixBend(uint32_t matrix) override;
    uint32_t tripMoving(uint8_t part) override;

protected:
    // `bytes` of the memory at rom.4's address `at`
    virtual void read(uint32_t at, void* to, size_t bytes) = 0;
    template <class T>
    T value(uint32_t at) {
        T v {};
        read(at, &v, sizeof v);
        return v;
    }
    uint8_t rd8(uint32_t at) { return value<uint8_t>(at); }
    uint16_t rd16(uint32_t at) { return value<uint16_t>(at); }
    uint32_t rd32(uint32_t at) { return value<uint32_t>(at); }
    // The part's N words for a child by `t`
    template <size_t N>
    std::array<uint16_t, N> partWords(const Vc2SetPartTable& t, uint32_t part, uint32_t kind, uint32_t index) {
        return value<std::array<uint16_t, N>>(t.at + t.part * part + t.kind * kind + sizeof(uint16_t) * N * index);
    }
};
