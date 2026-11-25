#pragma once
// rom.4's (the VC-2's) system store (v347): the three areas its store object 0x89961250 aims the system's read objects
// at, as one record. The system record 0x89add9a8 (104 bytes) holds system B's block at +0 (0x89320760: Part n's
// Receive Switch and Channel, the Receive flags, Patch Remain) and the system's block at +24 (0x89320790: Time Trip's
// assigns, the controls); system C's area 0x8996126d (56 bytes, the store object's +0x18: Out DC cut and Out Gain for
// TG, the voices' power) and system D's 0x8995cc14 (46 bytes, its +0: ChordMemory, the bank's record). An ID reads as
// rom.4's getter reads a real read object (0x8932b050 → 0x89328630): its bits at the area's base plus its descriptor's
// offset, the default when out of range, less the centre. The defaults are rom.4's own, run once in the image by
// vc2LoadSystem (0x89328d60 on the record, Part 1 Receive Switch on and part n on channel n among them; 0x89329190 on
// C, 0x89328d20 on D), as the XT starts with no "VC-2 System.s00" to load (0x89344860). Bit for bit: xt_vc2synth's
// start, load and notes parts. HARDWARE_FIRMWARE.md, "The part's system store".
#include <cstdint>
#include <vector>

struct Vc2System {
    static constexpr uint32_t kRecordAt = 0x89add9a8, kCAt = 0x8996126d, kDAt = 0x8995cc14;   // [FW]
    static constexpr int kRecordBytes = 104, kCBytes = 56, kDBytes = 46;   // [FW] each defaults routine's memset
    static constexpr int kRecord = 0, kC = kRecordBytes, kD = kC + kCBytes, kBytes = kD + kDBytes;
    // Each area: rom.4's address, its place in `bytes`, its size, its name
    struct Area {
        uint32_t at;
        int from, size;
        const char* what;
    };
    static constexpr Area kAreas[] = { { kRecordAt, kRecord, kRecordBytes, "the system record" },
                                       { kCAt, kC, kCBytes, "system C's area" },
                                       { kDAt, kD, kDBytes, "system D's area" } };
    std::vector<uint8_t> bytes = std::vector<uint8_t>(kBytes);
};

// A system block's area: system B's (the record's +0), the system's (its +24), C's and D's
enum class Vc2SystemArea : uint8_t { kB, kSystem, kC, kD };

// An ID on an area as rom.4's getter reads it
int vc2SystemValue(const Vc2System& s, Vc2SystemArea area, int id);
// v457: the other way: `value` (less the centre, as read) written as the ID's bits on the area
void vc2SystemPut(Vc2System& s, Vc2SystemArea area, int id, int value);
// v457: the area rom.4's readers read a system ID of the plugin's parameters on: Clock Source (the clock's, 0x89324350
// on 0x89320760) and External In Type (the system's dispatcher's, 0x8939b0d0) system B's; the controller assigns
// 0x89…0x96 (the assignments' dispatcher's 0x8939b110, Time Trip's) the system's.  None for Master Level and Output Gain,
// which no reader of the part reads (the effects' system reader 0x8935feb0 does: rom.4's unit, not in the VC-2 mode)
bool vc2SystemAreaOf(int id, Vc2SystemArea& area);
// v457: rom.4's system default of such an ID, less the centre, on its area; false for another ID or with none loaded
bool vc2SystemDefault(int id, int& value);

// rom.4's defaults for the three areas, run once in an image after its C startup; false when they fault. Not for the
// audio thread
[[nodiscard]] bool vc2LoadSystem(const char* path);
const Vc2System* vc2SystemDefaults();   // none until loaded
