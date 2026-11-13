// vc2run <rom.4> fxprog <rom.2>: the VC-2's reverb types and the effect chip's programs each one loads, run on rom.4's
// own routines (research/VC2_VOCAL_DESIGNER.md, "The effect chip's programs"), as fxmap.cpp's table runs rom.2's.  The
// chip's registers are plain memory here (0xb0000000), and its program loader (0x89362b60, rom.2's 0x894afa10(word, n,
// table)) is hooked to log each load and do nothing.  rom.4's reverb init (0x893873d0), then each type of its table set
// (0x89387490), its 24 parameters set to its typed defaults (0x893874a0, the second bank's), and applied (0x89387520).  Says,
// per type, its name (rom.4's list, 15 of them) and what it loads; and each 350-word program held to rom.2's eight reverb
// programs (fxmap's table) word by word: the closest, its words equal and those that differ in the register field alone.
// Fails on a fault.
#include "vc2patches.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace vc2 {

namespace {

// [FW] rom.4's; rom.2's 0x894d09b0, 0x894d0a70, 0x894d0a80, 0x894d0b00 (codepairs.py)
constexpr uint32_t kReverbInit = 0x893873d0, kSetType = 0x89387490, kSetParam = 0x893874a0, kApply = 0x89387520;
constexpr uint32_t kLoadProgram = 0x89362b60;                     // [FW] (word, n, table)
constexpr uint32_t kChip = 0xb0000000, kChipBytes = 0x4000;
constexpr uint32_t kTypeIds = 0x895165e4;                         // [FW] ("The second bank, read")
constexpr uint32_t kTypeNames = 0x89577ee0, kNameBytes = 20, kNamedTypes = 15;      // [FW] "00:Off", …
constexpr int kTypes = 17, kParams = 24;   // the table's entries; the parameters the setter takes
constexpr uint32_t kReverbWords = 350, kRom2Base = 0x89300000;
constexpr uint32_t kRegister = 0x3fc00;   // a program word's register field, bits 17:10

struct Rom2Program {
    const char* name;
    uint32_t at;   // [FW] rom.2's, fxmap's table
};
constexpr Rom2Program kRom2[] = {
    {"Off", 0x897bf948},        {"Room 1", 0x897bec24}, {"Room 2, Hall 2", 0x897bfed0},
    {"Room 3, Hall 3, Garage", 0x897c0930}, {"Hall 1", 0x897bdd78}, {"Plate", 0x897be664},
    {"Non-Linear", 0x897bf1e8}, {"Delay", 0x897bd7a4},
};

struct Load {
    uint32_t word, n, table;
};

uint32_t rom2Word(const std::vector<uint8_t>& rom2, uint32_t at) {
    uint32_t w = 0;
    std::memcpy(&w, &rom2[at - kRom2Base], 4);
    return w;
}

// rom.4's program at `table` against rom.2's eight: the closest, by words equal and those differing in the register alone
void sayMatch(Machine& m, const std::vector<uint8_t>& rom2, uint32_t table) {
    const Rom2Program* best = nullptr;
    int bestEqual = -1, bestRegister = 0;
    for (const Rom2Program& p : kRom2) {
        int equal = 0, reg = 0;
        for (uint32_t k = 0; k < kReverbWords; ++k) {
            const uint32_t a = rom2Word(rom2, p.at + 4 * k), b = m.cpu.rd32(table + 4 * k), x = a ^ b;
            equal += x == 0 ? 1 : 0;
            reg += x != 0 && (x & ~kRegister) == 0 ? 1 : 0;
        }
        if (equal + reg > bestEqual + bestRegister) {
            best = &p;
            bestEqual = equal;
            bestRegister = reg;
        }
    }
    std::printf("      closest rom.2 program: %s, %d of %u words equal, %d more differing in the register alone\n",
                best->name, bestEqual, kReverbWords, bestRegister);
}

std::string typeName(Machine& m, int type) {
    if (type >= (int)kNamedTypes) return "(past the list)";
    std::string s;
    for (uint32_t k = 0; k < kNameBytes; ++k) {
        const char c = (char)m.cpu.rd8(kTypeNames + kNameBytes * (uint32_t)type + k);
        if (!c) break;
        s += c;
    }
    return s;
}

// One type set with its typed defaults and applied; what it loaded, said; false on a fault
bool runType(Machine& m, const std::vector<uint8_t>& rom2, int type, std::vector<Load>& loads) {
    const int first = (int16_t)m.cpu.rd16(kTypeIds + 4 * (uint32_t)type);
    const int last = (int16_t)m.cpu.rd16(kTypeIds + 4 * (uint32_t)type + 2);
    loads.clear();
    if (!m.run(kSetType, {(uint32_t)type})) return false;
    for (int i = 0; i < kParams; ++i) {
        const int def = i <= last - first ? typedDescriptorOf(m, (uint32_t)(first + i)).def : 0;
        if (!m.run(kSetParam, {(uint32_t)i, (uint32_t)def})) return false;
    }
    if (!m.run(kApply, {0})) return false;
    std::printf("  type %2d %-20s typed IDs %03x…%03x:", type, typeName(m, type).c_str(), first, last);
    if (loads.empty()) std::printf(" no program loaded");
    for (const Load& l : loads) std::printf(" %u words at %03x from %#x;", l.n, l.word, l.table);
    std::printf("\n");
    for (const Load& l : loads)
        if (l.n == kReverbWords) sayMatch(m, rom2, l.table);
    return true;
}

} // namespace

int fxprog(Machine& m, int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "%s <rom.2>\n", argv[0]);
        return 2;
    }
    const std::vector<uint8_t> rom2 = fileContents(argv[1]);
    if (rom2.size() < 0x500000) {
        std::printf("fxprog: %s is not rom.2\n", argv[1]);
        return 1;
    }
    m.cpu.mapRam(kChip, kChipBytes);
    std::vector<Load> loads;
    m.cpu.hook(kLoadProgram, [&loads](Sh4& c) {
        loads.push_back({c.r[4] & 0xffff, c.r[5] & 0xffff, c.r[6]});
        c.ret(0);
    });
    std::printf("rom.4's reverb (init, then each type with its typed defaults; %d kernel stubs answered with 0)\n",
                answerKernel(m));
    bool ok = m.run(kReverbInit, {});
    for (int t = 0; t < kTypes && ok; ++t) ok = runType(m, rom2, t, loads);
    if (ok) return 0;
    std::printf("fxprog: faulted: %s\n", m.error().c_str());
    return 1;
}

} // namespace vc2
