// vc2run <rom.4> typed <file.p02> …: the VC-2's typed blocks (MFX, Chorus, Reverb, the two COSMs) read the firmware's way,
// through the second descriptor bank (research/VC2_VOCAL_DESIGNER.md, "The second descriptor bank, read").  Each file
// into rom.4's store (vc2patches' loadStore); each block's Type read by rom.4's reader (0x8932b050) at the block's place
// in the store (the firmware's pointing routines: MFX +0x6c, Chorus +0xb0, Reverb +0xcc, COSM 1 +0xd3e and COSM 2 +0xf5e
// by 34 a zone), its type's typed IDs from rom.4's tables of each type's first and last ID, and each typed value by
// rom.4's typed reader (0x8932b460(section, the block's slot base, id)).  Says which types the files use, and per typed ID
// its descriptor (the second bank: 8 bytes an ID from 0x56a, slot, default, min, max) and its values over the files whose
// block has its type: lo..hi, how many differ, how many are off the default and outside the range.  Fails on a fault.
#include "vc2patches.h"
#include <cstdio>
#include <map>
#include <vector>

namespace vc2 {

namespace {

constexpr uint32_t kTypedRead = 0x8932b460;                    // [FW] (section, slot base, id) → the value
constexpr uint32_t kBank = 0x895131a8, kFirstTyped = 0x56a;    // [FW] the second bank, 8 bytes an ID
constexpr uint32_t kCosmSwitchId = 0x462;                      // [FW]
constexpr uint32_t kNames = 2476;                              // [FW] the names table's IDs, both banks

struct Block {
    const char* name;
    uint32_t typeId, slotBase;   // [FW] its Type's ID, and the ID a typed descriptor's slot counts from
    uint32_t at, table;          // [FW] its place in the store (a pointing routine's), its types' first and last IDs
    uint32_t zones, zoneStride;  // 1 for the patch's own block; a COSM's, 16 zones 34 bytes apart
};
constexpr Block kBlocks[] = {
    {"MFX", 0x1b7, 0x1bb, 0x6c, 0x895164ec, 1, 0},
    {"Chorus", 0x23c, 0x23f, 0xb0, 0x895165b8, 1, 0},
    {"Reverb", 0x270, 0x272, 0xcc, 0x895165e4, 1, 0},
    {"COSM 1", 0x463, 0x464, 0xd3e, 0x8953fb40, 16, 34},
    {"COSM 2", 0x463, 0x464, 0xf5e, 0x8953fb40, 16, 34},
};
constexpr bool isCosm(const Block& b) {
    return b.zones > 1;
}

struct TypedDescriptor {
    int slot, def, min, max;
};

TypedDescriptor typedDescriptorOf(Machine& m, uint32_t id) {
    const uint32_t d = kBank + 8 * (id - kFirstTyped);
    return {m.cpu.rd8(d), (int16_t)m.cpu.rd16(d + 2), (int16_t)m.cpu.rd16(d + 4), (int16_t)m.cpu.rd16(d + 6)};
}

// A type's first and last typed IDs, from its block's table
std::pair<uint32_t, uint32_t> typeIds(Machine& m, const Block& b, int type) {
    const uint32_t at = b.table + 4 * (uint32_t)type;
    return {(uint32_t)(int16_t)m.cpu.rd16(at), (uint32_t)(int16_t)m.cpu.rd16(at + 2)};
}

int typeCount(Machine& m, const Block& b) {
    const Descriptor d = descriptorOf(m, b.typeId);
    return d.max - d.centre + 1;
}

// One block's use over the files: how many use each type, and each typed ID's values
struct BlockUse {
    std::map<int, int> types;
    std::map<uint32_t, std::vector<int>> values;
    int off = 0;   // zones whose COSM switch is off
};

bool readValue(Machine& m, uint32_t section, uint32_t id, int& out) {
    uint32_t r = 0;
    if (!m.run(kReadValue, {section, id}, &r)) return false;
    out = (int32_t)r;
    return true;
}

// One file's block (one zone of it): its type, and its type's typed values; false on a fault
bool readBlock(Machine& m, const Block& b, uint32_t zone, BlockUse& use) {
    const uint32_t section = kPatchStore + b.at + b.zoneStride * zone;
    int type = 0, on = 1;
    if (!readValue(m, section, b.typeId, type)) return false;
    if (isCosm(b) && !readValue(m, section, kCosmSwitchId, on)) return false;
    if (!on) {
        ++use.off;
        return true;
    }
    ++use.types[type];
    const auto [first, last] = typeIds(m, b, type);
    for (uint32_t id = first; id <= last; ++id) {
        uint32_t r = 0;
        if (!m.run(kTypedRead, {section, b.slotBase, id}, &r)) return false;
        use.values[id].push_back((int32_t)r);
    }
    return true;
}

// One typed ID's line; true when a value is outside its range
bool sayTyped(Machine& m, uint32_t id, const std::vector<int>& v) {
    const TypedDescriptor d = typedDescriptorOf(m, id);
    const ValueSpread s = spreadOf(v, d.def, d.min, d.max);
    std::printf("    %03x %-28s slot %2d %6d…%-6d %6d | %6d…%-6d %3ld %3ld %3ld of %zu\n", id, paramName(m, id).c_str(),
                d.slot, d.min, d.max, d.def, s.lo, s.hi, s.distinct, s.off, s.outside, v.size());
    return s.outside > 0;
}

long sayBlock(Machine& m, const Block& b, const BlockUse& use) {
    std::printf("\n%s (Type %03x, %d types; slots from %03x; at +%#x", b.name, b.typeId, typeCount(m, b), b.slotBase,
                b.at);
    if (isCosm(b)) std::printf(", zones 34 apart; %d zones with the switch off", use.off);
    std::printf(")\n  types used:");
    for (const auto& [type, n] : use.types) std::printf(" %d ×%d", type, n);
    std::printf("\n    ID  name                         slot  range         default | values: lo…hi, distinct, off "
                "the default, outside the range\n");
    long outside = 0;
    for (const auto& [id, v] : use.values) outside += sayTyped(m, id, v) ? 1 : 0;
    return outside;
}

void sayTables(Machine& m) {
    std::printf("the second bank from %#x, IDs %#x… (the names run to %#x)\n", kBank, kFirstTyped, kNames - 1);
    for (const Block& b : kBlocks) {
        const uint32_t first = typeIds(m, b, 0).first, last = typeIds(m, b, typeCount(m, b) - 1).second;
        std::printf("  %-7s %2d types at %#x: typed IDs %03x…%03x\n", b.name, typeCount(m, b), b.table, first, last);
    }
}

} // namespace

int typed(Machine& m, int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "%s <file.p02> …\n", argv[0]);
        return 2;
    }
    sayTables(m);
    std::vector<BlockUse> uses(std::size(kBlocks));
    for (int k = 1; k < argc; ++k) {
        const std::vector<uint8_t> data = p02Data(argv[k]);
        bool ok = !data.empty() && loadStore(m, data);
        for (size_t b = 0; b < std::size(kBlocks) && ok; ++b)
            for (uint32_t z = 0; z < kBlocks[b].zones && ok; ++z) ok = readBlock(m, kBlocks[b], z, uses[b]);
        if (!ok) {
            std::printf("%s: not read: %s\n", argv[k], m.error().c_str());
            return 1;
        }
    }
    long outside = 0;
    for (size_t b = 0; b < std::size(kBlocks); ++b) outside += sayBlock(m, kBlocks[b], uses[b]);
    std::printf("\ntyped IDs with a value outside the range: %ld\n", outside);
    return 0;
}

} // namespace vc2
