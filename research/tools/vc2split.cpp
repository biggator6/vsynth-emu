// vc2run <rom.4> splits <index>: how a VC-2 wave's split is chosen when it plays (research/VC2_VOCAL_DESIGNER.md, "A VC-2
// wave's split, chosen").  rom.4's index parser on the VC-2's WLF08 index (vc2index.h); then the two readers the PCM converter's new branch (0x8939cba0) calls: each wave's (0x89394b30(wave, velocity, out): out+6
// the map, out+4 the split's second byte) at every velocity 0…127, and each map's (0x89394c20(map, key, out): out+6 the
// sample, out+4 and +5 its second and third bytes) at every key, printed as `wlflib.py choices VC2` prints the rule's, so
// the two diff to nothing when the readers keep it.  And the key a map is read at, the key-follow key (0x8939c910, rom.2's
// 0x894e12c0), against the port's xtKeyFollowKey over keys, coarse tunes and keyfollows.  Fails on a fault, a reader that
// finds nothing, or a key that differs.
#include "XtOsc.h"
#include "vc2index.h"
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

namespace vc2 {

namespace {

constexpr uint32_t kWaveRecord = 0x89989754, kMapRecord = 0x89989964;  // [FW] where each reader reads its record
constexpr uint32_t kKeyFollowKey = 0x8939c910;                  // [FW] (key, coarse, keyfollow, centre 60)
constexpr int kValues = 128, kNameBytes = 16, kCentre = 60;

std::string trimmed(std::string s) {
    s.erase(s.find_last_not_of(' ') + 1);
    return s;
}

// One wave's or map's choice at each value, as wlflib.py's say_ranges prints it; false when a value finds none or faults
bool sayRanges(const char* what, int number, const std::string& name, const std::function<std::string(int)>& at) {
    std::string line, last;
    int from = 0;
    bool ok = true;
    for (int v = 0; v <= kValues; ++v) {
        const std::string r = v < kValues ? at(v) : std::string();
        ok = ok && r != "none";
        if (v > 0 && r == last) continue;
        if (v > 0) line += (line.empty() ? "" : ", ") + std::to_string(from) + "-" + std::to_string(v - 1) + " " + last;
        from = v;
        last = r;
    }
    std::printf("%s %3d %-16s %s\n", what, number, name.c_str(), line.c_str());
    return ok;
}

// A reader's choice at one value: its out fields as text, or "none"
std::string choiceOf(Machine& m, uint32_t reader, int number, int value, uint32_t out, bool map) {
    uint32_t found = 0;
    if (!m.run(reader, {(uint32_t)number, (uint32_t)value, out}, &found) || !found) return "none";
    char s[48];
    const unsigned chosen = m.cpu.rd16(out + 6), second = m.cpu.rd8(out + 4), third = m.cpu.rd8(out + 5);
    if (map)
        std::snprintf(s, sizeof s, "sample %u (byte %u, %u)", chosen, second, third);
    else
        std::snprintf(s, sizeof s, "map %u (byte %u)", chosen, second);
    return s;
}

// Each sample by the third reader (0x89394d10(sample, out)): out+2 the id the PCM converter takes to the wave store (a
// V-Synth wave's is 2999 + its number); out+0 the byte it passes on.  Consecutive samples a step apart as one range
bool saySamples(Machine& m, uint32_t out) {
    const int samples = m.cpu.rd16(kSampleCount);
    std::printf("samples (%d): number → the wave store's id:", samples);
    int runFrom = 0, runId = 0, last = -1;
    for (int s = 1; s <= samples + 1; ++s) {
        uint32_t found = 1;
        if (s <= samples && (!m.run(kSampleInfo, {(uint32_t)s, out}, &found) || !found)) return false;
        const int id = s <= samples ? (int)m.cpu.rd16(out + 2) : -1;
        if (s > 1 && id == last + 1) {
            last = id;
            continue;
        }
        if (s > 1) std::printf(" %d-%d → %d-%d;", runFrom, s - 1, runId, last);
        runFrom = s;
        runId = last = id;
    }
    std::printf(" out+0 at sample 1: %u\n", m.cpu.rd8(out));
    return true;
}

// The firmware's key-follow key against xtKeyFollowKey: how many cases, how many differ
bool keysAlike(Machine& m) {
    long cases = 0, differ = 0;
    for (int key = 0; key < kValues; ++key)
        for (int coarse = -48; coarse <= 48; coarse += 3)
            for (int kf = -2000; kf <= 2000; kf += 125) {
                uint32_t r = 0;
                if (!m.run(kKeyFollowKey, {(uint32_t)key, (uint32_t)coarse, (uint32_t)kf, kCentre}, &r)) return false;
                ++cases;
                differ += (int)(r & 0xff) != xtKeyFollowKey(key, coarse, kf) ? 1 : 0;
            }
    std::printf("the key-follow key (0x8939c910) against xtKeyFollowKey: %ld cases, %ld different\n", cases, differ);
    return differ == 0;
}

} // namespace

int splits(Machine& m, int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "%s <VC-2 Wave.bin, wlflib.py get VC2>\n", argv[0]);
        return 2;
    }
    IndexFile f{fileContents(argv[1])};
    const std::optional<int32_t> parsed = parseIndex(m, f, argv[0]);
    if (!parsed) return 1;
    const uint32_t out = m.alloc(64);
    const int waves = m.cpu.rd16(kWaveCount), maps = m.cpu.rd16(kMapCount);
    std::fprintf(stderr, "the parser: %d, %d waves, %d maps\n", *parsed, waves, maps);
    bool ok = waves > 0 && maps > 0;
    for (int w = 1; w <= waves; ++w) {
        const auto at = [&](int v) { return choiceOf(m, kWaveSplit, w, v, out, false); };
        at(0);
        ok = sayRanges("wave", w, trimmed(cString(m, kWaveRecord, kNameBytes)), at) && ok;
    }
    for (int k = 1; k <= maps; ++k) {
        const auto at = [&](int v) { return choiceOf(m, kMapSplit, k, v, out, true); };
        at(0);
        ok = sayRanges("map", k, trimmed(cString(m, kMapRecord, kNameBytes)), at) && ok;
    }
    ok = saySamples(m, out) && ok;
    ok = keysAlike(m) && ok;
    return ok ? 0 : 1;
}

} // namespace vc2
