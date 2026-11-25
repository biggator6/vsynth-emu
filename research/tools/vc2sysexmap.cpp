// vc2run sysex: rom.4's SysEx address map by its own code, through vc2sysex.h's probe (HARDWARE_FIRMWARE.md,
// "rom.4's SysEx: the VC-2's own address map").  Every message is a DT1 of model 00 00 0D to device 0x10 with 384 data
// bytes, byte k being k & 15, so an address's value shows how many bytes it takes.
//   sysex   1. at Category 1, b1 00, every b0 and b2: which b0 write anything;
//           2. on those, every b1 and b2: where they stop;
//           3. on those, b2 0…8 at every b1 at each Category 0…12: the map, each address with the Categories that
//              write it; a MIC setting's blocks against the first's, a stored patch's against the temporary's;
//           4. what the parser takes: the model, the device byte, the checksum;
//           5. what rom.4 sends: an RQ1's answer, and each kind's edit data for every ID (vc2sysexsend.cpp)
#include "vc2sysex.h"
#include <array>
#include <cstdio>
#include <set>

namespace vc2 {

namespace {

using sx::byteOf;
using sx::Dt1;
using sx::kSeven;
using sx::Map;
using sx::packed;
using sx::Probe;
using sx::Set;
using sx::Written;

constexpr uint32_t kSpan = 384;              // data bytes a message: three b2's
constexpr uint32_t kCategories = 13;         // 0…11 the descriptor's (0x12c), 12 past it
constexpr uint32_t kDevice = 0x8992e3d0;     // [FW] the device byte (.data), Device ID's (0x77) by 0x893154a0

using Shape = std::vector<std::array<uint32_t, 4>>;

void setAddress(Dt1& d, uint32_t address) {
    for (int k = 0; k < 4; ++k) d.address[k] = (uint8_t)byteOf(address, k);
}

Dt1 dt1At(uint32_t address) {
    Dt1 d;
    setAddress(d, address);
    for (uint32_t k = 0; k < kSpan; ++k) d.data.push_back((uint8_t)(k & 15));
    return d;
}

// One message's sets into the map at their addresses, an address's sets as the first message to write it gave them;
// false on a fault
bool sendAt(Probe& p, uint32_t address, Map& map) {
    std::map<uint32_t, std::vector<Set>> now;
    for (const Set& s : p.send(dt1At(address))) now[address + s.at].push_back(s);
    for (auto& [a, sets] : now) {
        Written& w = map[a];
        if (w.sets.empty()) w.sets = std::move(sets);
        w.categories |= 1u << p.category;
    }
    return !p.failed;
}

// Every b2 of one b0 and b1, 384 bytes a message
bool sweepB2(Probe& p, uint32_t b0, uint32_t b1, Map& map) {
    for (uint32_t b2 = 0; b2 <= kSeven; b2 += kSpan / 128)
        if (!sendAt(p, packed(b0, b1, b2, 0), map)) return false;
    return true;
}

std::set<uint32_t> firstBytes(const Map& map) {
    std::set<uint32_t> out;
    for (const auto& [a, w] : map) out.insert(byteOf(a, 0));
    return out;
}

void printCategories(uint32_t mask) {
    if (mask == (1u << kCategories) - 1) return (void)std::printf("all");
    if (mask == (1u << (kCategories - 1)) - 1) return (void)std::printf("0-11");
    const char* sep = "";
    for (uint32_t c = 0; c < kCategories; ++c) {
        if (!(mask >> c & 1)) continue;
        std::printf("%s%u", sep, c);
        sep = ",";
    }
}

void printLine(Machine& m, uint32_t a, const Written& w) {
    for (const Set& s : w.sets) {
        sx::printAddress(a);
        std::printf("  g%-2d %-6s", s.group, s.what);
        if (s.zone >= 0) std::printf(" zone %d", s.zone + 1);
        else std::printf("       ");
        std::printf("  0x%03x %-30s value 0x%04x block %u  Categories ", s.named, sx::idName(m, s.named), s.value,
                    s.block);
        printCategories(w.categories);
        if (s.id != s.named) std::printf("  (handed 0x%03x)", s.id);
        std::printf("\n");
    }
}

// What a block writes, relative to its start: (b2 b3, group, zone, named ID) for each set
Shape shape(const Map& map, uint32_t from, uint32_t to) {
    Shape out;
    for (auto it = map.lower_bound(from); it != map.end() && it->first < to; ++it) {
        for (const Set& s : it->second.sets)
            out.push_back({it->first - from, (uint32_t)s.group, (uint32_t)s.zone, s.named});
    }
    return out;
}

// Passes 1 and 2: the b0 that write, then their every b1 and b2, at Category 1
bool extent(Probe& p, Map& map) {
    for (uint32_t b0 = 0; b0 <= kSeven; ++b0)
        if (!sweepB2(p, b0, 0, map)) return false;
    const std::set<uint32_t> used = firstBytes(map);
    std::printf("1. b0 that write at b1 00 (any b2): ");
    for (const uint32_t b0 : used) std::printf("%02x ", b0);
    for (const uint32_t b0 : used)
        for (uint32_t b1 = 1; b1 <= kSeven; ++b1)
            if (!sweepB2(p, b0, b1, map)) return false;
    std::printf("\n2. where they write, every b1 and b2:\n");
    for (const uint32_t b0 : used) {
        std::set<uint32_t> b1s, b2s;
        for (const auto& [a, w] : map) {
            if (byteOf(a, 0) != b0) continue;
            b1s.insert(byteOf(a, 1));
            b2s.insert(byteOf(a, 2));
        }
        std::printf("   b0 %02x: b1 %02x…%02x (%zu of them), b2 %02x…%02x\n", b0, *b1s.begin(), *b1s.rbegin(),
                    b1s.size(), *b2s.begin(), *b2s.rbegin());
    }
    return true;
}

// Pass 3: b2 0…8 at every b1 of the b0 that write, at each Category
bool byCategory(Probe& p, const std::set<uint32_t>& used, Map& map) {
    for (uint32_t c = 0; c < kCategories; ++c) {
        p.category = c;
        for (const uint32_t b0 : used)
            for (uint32_t b1 = 0; b1 <= kSeven; ++b1)
                for (uint32_t b2 = 0; b2 < 9; b2 += kSpan / 128)
                    if (!sendAt(p, packed(b0, b1, b2, 0), map)) return false;
    }
    p.category = 1;
    return true;
}

void printMap(Machine& m, const Map& map) {
    std::printf("3. the map (the temporary patch, MIC setting 1, stored patch 20 00), each set the setter's own:\n");
    for (const auto& [a, w] : map) {
        const uint32_t b0 = byteOf(a, 0);
        if ((b0 >= 0x20 && a >> 14 != 0x20u << 7) || (b0 == 0x03 && byteOf(a, 2) > 0)) continue;
        printLine(m, a, w);
    }
}

// The number a block's first set carries (+0 of its info), 0 for none
uint32_t numberAt(const Map& map, uint32_t address) {
    const auto it = map.find(address);
    return it == map.end() ? 0 : it->second.sets.front().block;
}

// Each block of a repeated kind against its first: the MIC settings' 03 00 0i, the stored patches' against 10 00
void printRepeats(const Map& map) {
    const Shape mic = shape(map, packed(3, 0, 0, 0), packed(3, 0, 1, 0));
    std::printf("   MIC settings, 03 00 0i against 03 00 00 (%zu sets):", mic.size());
    for (uint32_t i = 0; i <= kSeven; ++i) {
        const Shape s = shape(map, packed(3, 0, i, 0), packed(3, 0, i + 1, 0));
        const char* verdict = s == mic ? "same" : "DIFFERS";
        if (!s.empty()) std::printf(" %u %s (block %u),", i, verdict, numberAt(map, packed(3, 0, i, 0)));
    }
    const Shape temporary = shape(map, packed(0x10, 0, 0, 0), packed(0x10, 1, 0, 0));
    std::printf("\n   stored patches from 20 00 00 00, a b1 each, against 10 00 (%zu sets):", temporary.size());
    uint32_t same = 0, numbered = 0, last = 0;
    for (uint32_t k = 0; k < 4 * 128; ++k) {
        const uint32_t from = packed(0x20 + k / 128, k % 128, 0, 0);
        const Shape s = shape(map, from, from + (1u << 14));
        if (s.empty()) continue;
        last = k;
        same += s == temporary;
        numbered += numberAt(map, from) == k + 65;
        if (s != temporary) std::printf(" %u DIFFERS,", k);
    }
    std::printf(" %u blocks, the last %u; %u the same, %u with patch number k + 65\n", last + 1, last, same, numbered);
}

// 4. One byte to Patch Name 1's address: how many sets, and the screen's messages
void tryOne(Probe& p, const char* what, Dt1 d, std::vector<uint8_t> data = {0x41}) {
    setAddress(d, packed(0x10, 0, 0, 1));
    d.data = std::move(data);
    const size_t screens = p.screen.size();
    const int model24 = p.model24;
    const size_t n = p.send(d).size();
    std::printf("   %-44s %zu set(s), model 00 24's DT1 %d, screen codes", what, n, p.model24 - model24);
    for (size_t k = screens; k < p.screen.size(); ++k) std::printf(" %u", p.screen[k]);
    std::printf("%s\n", p.failed ? " (the run stopped)" : "");
    p.failed = false;
}

void checks(Probe& p) {
    std::printf("4. what the parser takes, one byte to 10 00 00 01:\n");
    const Dt1 base;
    tryOne(p, "model 00 00 0D, device 10", base);
    Dt1 d = base;
    for (const uint8_t model : {0x53, 0x4d}) {
        d.model = {0x00, model};
        char what[48];
        std::snprintf(what, sizeof what, "model 00 %02x", model);
        tryOne(p, what, d);
    }
    d.model = {0x00, 0x24};
    tryOne(p, "model 00 24, a fifth address byte 00", d, {0x00, 0x41});
    d = base;
    for (const uint8_t device : {0x11, 0x1f, 0x7f}) {
        d.device = device;
        char what[48];
        std::snprintf(what, sizeof what, "device %02x (the byte 10)", device);
        tryOne(p, what, d);
    }
    p.m.cpu.wr8(kDevice, 0x1f);
    d.device = 0x1f;
    tryOne(p, "device 1f, the byte set to 1f", d);
    d.device = 0x10;
    tryOne(p, "device 10, the byte set to 1f", d);
    p.m.cpu.wr8(kDevice, sx::kDeviceDefault);
    d = base;
    d.badSum = true;
    tryOne(p, "the checksum one off", d);
}

} // namespace

int sysex(Machine& m, int, char**) {
    Probe p{m};
    if (!p.start()) {
        std::fprintf(stderr, "sysex: the receive object wasn't built\n");
        return 1;
    }
    Map map;
    if (!extent(p, map) || !byCategory(p, firstBytes(map), map)) return 1;
    printMap(m, map);
    printRepeats(map);
    checks(p);
    sx::rq1(p);
    sx::sends(p, map);
    std::printf("%llu steps\n", (unsigned long long)m.cpu.steps());
    return 0;
}

} // namespace vc2
