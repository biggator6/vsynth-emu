// vc2run sysex's send side (vc2sysex.h): what rom.4 puts on MIDI out A.  An RQ1 through the parser, then each kind's
// edit-data sender (0x89315f90 and its siblings, which the parameter writers call while Transmit Edit Data is on:
// 0x8932164a and the rest) for every ID, MIDI out A's ring read back after each.  The values a message carries are
// the read hook's answer (the Category), not a patch's: only its head, address and length mean anything.
#include "vc2sysex.h"
#include <array>
#include <cstdio>
#include <set>

namespace vc2::sx {

namespace {

constexpr uint32_t kMap = 0x8995e644;   // [FW] the object the senders are methods of
constexpr uint32_t kIds = 0x9ac;        // [FW] rom.4's IDs (VC2_VOCAL_DESIGNER.md, "The parameter tables")
constexpr size_t kHead = 11;            // F0 41, the device, 00 00 0D, 12, four address bytes

// A kind's sender: (this, first ID, last ID, a MIC setting's number from 1 in r7), a zoned one's zone on the stack
struct Sender {
    uint32_t at;
    int kind;
    const char* what;
    bool zoned, mic;
};
constexpr Sender kSenders[] = {
    {0x89315f90, 1, "setup", false, false},  {0x89315fb0, 2, "system", false, false},   // [FW]
    {0x89315fd0, 3, "system", false, false}, {0x89315ff0, 4, "MIC", false, true},       // [FW]
    {0x89316020, 5, "common", false, false}, {0x89316040, 7, "MFX", false, false},      // [FW]
    {0x89316060, 8, "Chorus", false, false}, {0x89316080, 9, "Reverb", false, false},   // [FW]
    {0x893160d0, 10, "env", true, false},    {0x893160a0, 11, "OSC", true, false},      // [FW]
    {0x89316110, 12, "COSM", true, false}};                                             // [FW]

struct Sent {
    uint32_t address, bytes;
    bool vc2;   // F0 41, any device, 00 00 0D, DT1
};

std::vector<Sent> dt1s(const std::vector<uint8_t>& b) {
    std::vector<Sent> out;
    for (size_t i = 0; i < b.size(); ++i) {
        if (b[i] != 0xf0) continue;
        size_t e = i + 1;
        while (e < b.size() && b[e] != 0xf7) ++e;
        if (e >= b.size() || e - i < kHead + 2) continue;
        const bool vc2 = b[i + 1] == 0x41 && b[i + 3] == 0 && b[i + 4] == 0 && b[i + 5] == 0x0d && b[i + 6] == 0x12;
        out.push_back({packed(b[i + 7], b[i + 8], b[i + 9], b[i + 10]), (uint32_t)(e - i - kHead - 1), vc2});
        i = e;
    }
    return out;
}

using In = std::map<std::pair<uint32_t, uint32_t>, std::set<uint32_t>>;   // (group, named ID) → addresses, any zone
using Out = std::map<uint32_t, std::vector<Sent>>;                          // ID → what its sender put out

// The receive map's addresses: the setup, the system, MIC setting 1, the temporary patch
In received(const Map& map) {
    In out;
    for (const auto& [a, w] : map) {
        if (byteOf(a, 0) >= 0x20 || (byteOf(a, 0) == 3 && byteOf(a, 2) > 0)) continue;
        for (const Set& s : w.sets) out[{(uint32_t)s.group, s.named}].insert(a);
    }
    return out;
}

// One sender on every ID: what it puts out, by ID; false on a fault
bool sweep(Probe& p, const Sender& s, uint32_t extra, Out& out) {
    for (uint32_t id = 0; id < kIds; ++id) {
        p.m.cpu.wr32(kStack, extra);
        if (!p.m.run(s.at, {kMap, id, id, extra})) return false;
        std::vector<Sent> sent = dt1s(p.drain());
        if (!sent.empty()) out[id] = std::move(sent);
    }
    return true;
}

// How an ID went out against the receive map: one DT1 of model 00 00 0D at the address the ID is received at (own),
// from an earlier address that the run covers (covers), at the address a value of the group is received at for an ID
// the map names no address (whole: a value's other byte, sent whole), or anything else
enum How { kOwn, kCovers, kWhole, kElsewhere, kHows };

How howSent(const In& in, uint32_t group, uint32_t id, const std::vector<Sent>& sent) {
    if (sent.size() != 1 || !sent[0].vc2) return kElsewhere;
    const Sent& s = sent[0];
    const auto it = in.find({group, id});
    if (it == in.end()) {
        for (const auto& [key, addresses] : in)
            if (key.first == group && addresses.count(s.address)) return kWhole;
        return kElsewhere;
    }
    if (it->second.count(s.address)) return kOwn;
    for (const uint32_t a : it->second)
        if (a > s.address && a < s.address + s.bytes) return kCovers;
    return kElsewhere;
}

void listId(Probe& p, uint32_t id, const std::vector<Sent>* sent) {
    std::printf("      0x%03x %-28s", id, idName(p.m, id));
    if (sent && !sent->empty()) {
        std::printf(" sent at ");
        printAddress(sent->front().address);
        std::printf(", %u bytes, %zu message(s)", sent->front().bytes, sent->size());
    }
    std::printf("\n");
}

void compare(Probe& p, const Sender& s, uint32_t extra, const Out& out, const In& in) {
    int n[kHows] = {}, notSent = 0;
    std::vector<uint32_t> odd, unsent;
    for (const auto& [id, sent] : out) {
        const How how = howSent(in, (uint32_t)s.kind, id, sent);
        ++n[how];
        if (how == kElsewhere) odd.push_back(id);
    }
    for (const auto& [key, addresses] : in) {
        if (key.first != (uint32_t)s.kind || out.count(key.second)) continue;
        ++notSent;
        unsent.push_back(key.second);
    }
    const char* which = s.zoned ? "the stack word" : (s.mic ? "MIC setting" : "");
    std::printf("   %-6s kind %2d %s %u: own address %d, covering it %d, a value sent whole %d, elsewhere %d; "
                "received but not sent %d\n", s.what, s.kind, which, extra, n[kOwn], n[kCovers], n[kWhole],
                n[kElsewhere], notSent);
    for (size_t k = 0; k < odd.size() && k < 8; ++k) listId(p, odd[k], &out.at(odd[k]));
    for (size_t k = 0; k < unsent.size() && k < 8; ++k) listId(p, unsent[k], nullptr);
}

} // namespace

void rq1(Probe& p) {
    Dt1 d;
    d.command = 0x11;
    d.address[0] = 0x10;
    d.address[3] = 1;
    d.data = {0, 0, 0, 4};
    p.drain();
    p.send(d);
    const std::vector<uint8_t> b = p.drain();
    std::printf("5. what rom.4 sends\n   an RQ1 for 4 bytes from 10 00 00 01, a word at a time, on MIDI out A:");
    for (const uint8_t byte : b) std::printf(" %02x", byte);
    std::printf("%s\n   on USB out:", p.failed ? " (the run stopped)" : "");
    for (const uint8_t byte : p.usb) std::printf(" %02x", byte);
    std::printf("\n");
    p.failed = false;
}

bool sameAddresses(const Out& a, const Out& b) {
    if (a.size() != b.size()) return false;
    for (const auto& [id, sent] : a) {
        const auto it = b.find(id);
        if (it == b.end() || it->second.size() != sent.size() || it->second[0].address != sent[0].address) return false;
    }
    return true;
}

void sends(Probe& p, const Map& map) {
    const In in = received(map);
    std::printf("   each kind's edit-data sender on every ID, against the receive map:\n");
    for (const Sender& s : kSenders) {
        std::vector<Out> outs;
        for (const uint32_t extra : s.zoned ? std::vector<uint32_t>{0, 1} : std::vector<uint32_t>{s.mic ? 1u : 0u}) {
            Out out;
            if (!sweep(p, s, extra, out)) {
                std::printf("   %s: the run stopped\n", s.what);
                continue;
            }
            compare(p, s, extra, out, in);
            outs.push_back(std::move(out));
        }
        if (outs.size() == 2) std::printf("      the stack word 0 and 1 send at the same addresses: %s\n",
                                          sameAddresses(outs[0], outs[1]) ? "yes" : "no");
    }
}

} // namespace vc2::sx
