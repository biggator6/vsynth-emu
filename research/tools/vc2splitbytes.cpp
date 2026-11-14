// vc2run <rom.4> splitbytes <index>: what reads the bytes a VC-2 wave's splits pass on (research/VC2_VOCAL_DESIGNER.md,
// "The split's bytes, followed").  The PCM converter's new branch (0x8939cba0) leaves them in the OSC's per-note input
// beside the wave's chunks: +0xa0 1 when the wave and its map have one split each, +0xa4 the wave split's second byte,
// +0xa5 the low byte of the map split's word (0 or 12), +0xa6 the map split's second byte, +0xa7 the sample's kind; for a
// V-Synth wave its fetch (0x8939cd30) writes 1, 127, 0, 127 and the wave's kind there.  rom.4's default patch with OSC 1
// on PCM (vc2oscinput.h), the index read by rom.4's parser and its readers over every wave, velocity and key
// (vc2index.h); the wave library and the wave store answered (vc2splitbytes.h; held 0x89351840, chunk 0x89351ad0, band
// 0x89351ff0, frames 0x89351a10) with one wave's chunks laid as xt_tone lays them (a header, a vpno of 8 events, a loop
// record, a zero vpdt and cached events, silent samples) for every sample 1…192, so the index's own readers choose the
// split and only the input's bytes differ between cases.  Each case: the dispatch (the fetch inside it) and two converter
// runs under a read-watch (vc2watch.h) of the input's +0xa0…+0xff over the tone generator's code 0x89380000…0x89400000;
// says the bytes the fetch left and every read of them.  Then each byte set to other values after the dispatch, the
// converter run twice, and the block compared word by word with the case's own: on a LITE sample, and +0xa0 on a SOLO
// sample under RETRIGGER, STEP and EVENT; +0xa7 is the control, which must move the block.  Then the laws
// (vc2splitlaws.cpp).  Fails on a fault, a fetch that fails, a control that moves nothing, or a law that differs.
#include "vc2index.h"
#include "vc2splitbytes.h"
#include "vc2watch.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <map>
#include <vector>

namespace vc2 {

namespace {

constexpr uint32_t kLibrary = 0x893520a0;   // [FW] (2999 + the Wave Number) → a VC-2 wave, when the store hasn't it
constexpr uint32_t kVSynthIds = 2999;       // [FW] a V-Synth wave's id less its number
constexpr uint32_t kHeld = 0x89351840, kChunk = 0x89351ad0, kBand = 0x89351ff0, kFrames = 0x89351a10;   // [FW] the store
constexpr uint32_t kWaveNumberId = 0x2fe, kPlaybackModeId = 0x312, kPcmType = 1;   // [FW] OSC 1's; OSC Type's PCM
constexpr uint32_t kWatchFrom = 0x89380000, kWatchTo = 0x89400000;   // the tone generator's code, the vocoder's with it
constexpr uint32_t kRegion = 0xa0, kRegionBytes = 0x60;               // [FW] the input's wave fields (0x8939cba0's r5)
constexpr uint32_t kOneSplit = 0xa0, kKind = 0xa7;                    // [FW] in the input
constexpr uint32_t kChunks = 11, kBands = 7, kHeader = 2, kSteps = 3, kLoop = 4, kVpdt = 5, kEvents = 10;   // [FW]
constexpr uint32_t kSoloKind = 1, kRate = 44000, kRootKey = 60, kFramesHeld = 0x8000;
constexpr uint32_t kVpnoEvents = 8, kEventStep = 1000;   // the vpno laid: 8 events, 1000 samples apart
constexpr uint32_t kOut = kScratch + 0x23000, kWave = kScratch + 0x30000, kReaderOut = kScratch + 0x24000;
constexpr int kFirstOffsets = 6;        // a differing block's offsets said
constexpr size_t kListedSamples = 20;   // a kind with this many samples or fewer has them listed

// [COR] from `vc2run … splits`: wave 1 LargeCho A S, one split each; wave 43 FMSynWave 1, its split's byte 88; wave 34
// Bs+Male+Fem1, key 36 in map 12's sample 130 with 12; wave 4 LargeChoVS S at velocity 100, its second split of three;
// wave 47 Male Laa, map 36's one sample 179, a SOLO sample
constexpr std::array kCases{SplitCase{1, "one split each", 60, 100}, SplitCase{43, "the wave split's byte 88", 60, 100},
                            SplitCase{34, "the map split's word 12", 36, 100},
                            SplitCase{4, "a wave of three splits", 60, 100}};
constexpr SplitCase kVariCase{47, "Male Laa, SOLO sample 179", 60, 100};
constexpr std::array<uint8_t, 2> kVariKeys{60, 64};   // EVENT by key counts from C4: event 0, and event 4
constexpr std::array kModes{std::pair{0, "RETRIGGER"}, std::pair{2, "STEP"}, std::pair{3, "EVENT"}};   // [FW] 0x312's
struct Poke {
    uint32_t at;
    const char* what;
    std::array<uint8_t, 3> values;
};
constexpr std::array kPokes{Poke{kOneSplit, "one split", {0, 2, 0xff}},
                            Poke{kWaveSplitByte, "the wave split's byte", {0, 64, 100}},
                            Poke{kShiftByte, "the map split's word", {12, 24, 0xf4}},
                            Poke{kMapSplitByte, "the map split's byte", {0, 64, 100}},
                            Poke{kKind, "the kind (control)", {1, 3, 5}}};
constexpr Poke kVariPoke{kOneSplit, "one split", {0, 2, 1}};

struct StoreWave {
    std::array<uint32_t, kChunks> chunk{};
    std::array<uint32_t, kBands> band{};
};

// One wave's chunks, as xt_tone's layoutWave lays a wave with no records (its vpno as writeVpno writes one: +0 8, +4 the
// count, +0xa the count less one, then the events from +0xc)
StoreWave layWave(Machine& m) {
    StoreWave w;
    uint32_t next = kWave;
    const auto put = [&](uint32_t bytes) {
        zeroBytes(m, next, bytes);
        const uint32_t at = next;
        next += (bytes + 0xfff) & ~0xfffu;
        return at;
    };
    w.chunk[kHeader] = put(0x40);
    w.chunk[kSteps] = put(0x40);
    w.chunk[kLoop] = put(0x40);
    w.chunk[kVpdt] = put(0x400);
    w.chunk[kEvents] = put(0x400);
    w.band[0] = put(kFramesHeld * 2);
    m.cpu.wr8(w.chunk[kHeader] + 1, kSoloKind);
    m.cpu.wr16(w.chunk[kHeader] + 8, kRate);
    m.cpu.wr8(w.chunk[kHeader] + 0xa, kRootKey);
    m.cpu.wr32(w.chunk[kSteps], 8);
    m.cpu.wr32(w.chunk[kSteps] + 4, kVpnoEvents);
    m.cpu.wr16(w.chunk[kSteps] + 0xa, kVpnoEvents - 1);
    for (uint32_t e = 0; e < kVpnoEvents; ++e) m.cpu.wr16(w.chunk[kSteps] + 0xc + 2 * e, e * kEventStep);
    m.cpu.wr8(w.chunk[kLoop] + 0x1a, 1);
    return w;
}

void answerStore(Machine& m, const StoreWave& w, int samples) {
    m.cpu.hook(kHeld, [samples](Sh4& c) { c.ret(c.r[4] >= 1 && (int)c.r[4] <= samples ? 1 : 0); });
    m.cpu.hook(kChunk, [w](Sh4& c) { c.ret(c.r[5] < kChunks ? w.chunk[c.r[5]] : 0); });
    m.cpu.hook(kBand, [w](Sh4& c) { c.ret(c.r[5] < kBands ? w.band[c.r[5]] : 0); });
    m.cpu.hook(kFrames, [](Sh4& c) { c.ret(kFramesHeld); });
    m.cpu.hook(kLibrary, [](Sh4& c) { c.ret(c.r[4] - kVSynthIds); });
}

void sayReads(const ReadWatch& watch) {
    if (watch.reads().empty()) std::printf("    no reads of +0x%02x…+0x%02x\n", kRegion, kRegion + kRegionBytes - 1);
    for (const auto& [where, s] : watch.reads())
        std::printf("    +0x%02x w%u by %08x (%ld×)\n", kRegion + where.second, s.width, where.first, s.times);
}

// The block's words that differ from `base`: how many, and the first few offsets
int saySame(const Bytes& base, const Bytes& block) {
    int differ = 0;
    std::printf(" ");
    for (uint32_t at = 0; at < kBlockBytes; at += 4) {
        if (std::equal(&base[at], &base[at] + 4, &block[at])) continue;
        if (++differ <= kFirstOffsets) std::printf(" +0x%03x %08x→%08x", at, wordAt(base, at), wordAt(block, at));
    }
    std::printf(differ ? " (%d words differ)\n" : " the block the same\n", differ);
    return differ;
}

// The block's words that point into the input (through which a player could read it later)
void sayPointers(const Bytes& block) {
    int n = 0;
    for (uint32_t at = 0; at < kBlockBytes; at += 4) {
        const uint32_t w = wordAt(block, at);
        if (w < kSplitNote.input || w >= kSplitNote.input + kInputBytes) continue;
        std::printf("  block +0x%03x points at input +0x%03x\n", at, w - kSplitNote.input);
        ++n;
    }
    if (!n) std::printf("  no block word points into the input\n");
}

// The samples by kind, the rarer kinds' listed; then every wave, velocity and key counted by the kind it reaches and the
// one-split byte the fetch would leave (the wave's and the map's out+0 both set)
void sayReach(const IndexChoices& c) {
    std::map<int, std::vector<int>> of;
    for (size_t s = 1; s < c.kind.size(); ++s) of[c.kind[s]].push_back((int)s);
    std::printf("the samples' kind byte (0x89394d10's out+0):");
    for (const auto& [kind, list] : of) {
        std::printf(" %d ×%zu", kind, list.size());
        if (list.size() > kListedSamples) continue;
        for (const int s : list) std::printf("%s%d", s == list.front() ? " (samples " : " ", s);
        std::printf(")");
    }
    std::map<std::pair<int, int>, long> reach;
    for (size_t w = 1; w < c.waveMap.size(); ++w)
        for (int v = 0; v < kSplitValues; ++v)
            for (int key = 0; key < kSplitValues; ++key) {
                const int map = c.waveMap[w][v], one = c.waveOne[w][v] && c.mapOne[map][key] ? 1 : 0;
                ++reach[{c.kind[c.mapSample[map][key]], one}];
            }
    std::printf("\nevery wave × velocity × key by the kind it reaches and +0xa0:");
    for (const auto& [at, n] : reach) std::printf(" kind %d with %d ×%ld;", at.first, at.second, n);
    std::printf("\n");
}

int faulted(Machine& m) {
    std::printf("splitbytes: faulted: %s\n", m.error().c_str());
    return 1;
}

// A case under the watch: the bytes the fetch left, the block's kind, every read; false on a fault or no kind
bool watchedCase(Machine& m, ReadWatch& watch, const SplitCase& k, Bytes& block) {
    Bytes record;
    watch.watch(kSplitNote.input + kRegion, kSplitNote.input + kRegion + kRegionBytes);
    if (!recordFor(m, k.wave, record) || !runSplitNote(m, record, k, {}, block)) return false;
    const uint32_t in = kSplitNote.input;
    std::printf("wave %d (%s), key %u, velocity %u: +0xa0 %u, +0xa4 %u, +0xa5 %u, +0xa6 %u, +0xa7 %u; kind %u\n",
                k.wave, k.why, k.key, k.velocity, m.cpu.rd32(in + kOneSplit), m.cpu.rd8(in + kWaveSplitByte),
                m.cpu.rd8(in + kShiftByte), m.cpu.rd8(in + kMapSplitByte), m.cpu.rd8(in + kKind), wordAt(block, 0));
    sayReads(watch);
    return wordAt(block, 0) != 0;
}

// A poke's values on a case, each block against the case's own: how many words the last one moved; -1 on a fault
int pokeCase(Machine& m, const SplitCase& k, const Poke& p, const Bytes& base) {
    Bytes record, block;
    if (!recordFor(m, k.wave, record)) return -1;
    int least = kBlockBytes;
    for (const uint8_t v : p.values) {
        if (!runSplitNote(m, record, k, {{p.at, v}}, block)) return -1;
        std::printf("  +0x%02x (%s) %u:", p.at, p.what, v);
        least = std::min(least, saySame(base, block));
    }
    return least;
}

} // namespace

uint32_t wordAt(const Bytes& b, uint32_t at) {
    return (uint32_t)b[at] | (uint32_t)b[at + 1] << 8 | (uint32_t)b[at + 2] << 16 | (uint32_t)b[at + 3] << 24;
}

bool recordFor(Machine& m, int wave, Bytes& record) {
    return setToneValue(m, kWaveNumberId, wave) && buildRecord(m, kOut, 0, record);
}

bool runSplitNote(Machine& m, const Bytes& record, const SplitCase& k, InputPokes pokes, Bytes& block) {
    const OscNote& at = kSplitNote;
    writeOscNote(m, at, 0, k.key, k.velocity, record);
    if (!m.run(kDispatch, {at.input, at.block})) return false;
    for (const auto& [offset, value] : pokes) m.cpu.wr8(at.input + offset, value);
    if (!m.run(kConverter, {at.input, at.block}) || !m.run(kConverter, {at.input, at.block})) return false;
    block = bytesAt(m, at.block, kBlockBytes);
    return true;
}

int splitbytes(Machine& m, int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "%s <VC-2 Wave.bin, wlflib.py get VC2>\n", argv[0]);
        return 2;
    }
    ReadWatch watch(m, kWatchFrom, kWatchTo);
    IndexFile f{fileContents(argv[1])};
    if (!parseIndex(m, f, argv[0])) return 1;
    const std::optional<IndexChoices> choices = indexChoices(m, kReaderOut);
    if (!choices) return faulted(m);
    sayReach(*choices);
    answerStore(m, layWave(m), m.cpu.rd16(kSampleCount));
    if (!useDefaultPatch(m) || !setOscType(m, kPcmType)) return faulted(m);
    std::printf("watching %ld loads and branches in %08x…%08x\n", watch.hooked, kWatchFrom, kWatchTo);
    bool ok = true;
    Bytes base, block;
    for (const SplitCase& k : kCases) {
        if (!watchedCase(m, watch, k, block)) return faulted(m);
        if (base.empty()) base = block;
    }
    sayPointers(base);
    std::printf("on wave %d, each byte set after the fetch:\n", kCases[0].wave);
    for (const Poke& p : kPokes) {
        const int moved = pokeCase(m, kCases[0], p, base);
        if (moved < 0) return faulted(m);
        ok = ok && (p.at != kKind || moved > 0);
    }
    for (const auto& [mode, name] : kModes)
        for (const uint8_t key : kVariKeys) {
            SplitCase k = kVariCase;
            k.key = key;
            std::printf("Playback Mode %s, ", name);
            if (!setToneValue(m, kPlaybackModeId, mode) || !watchedCase(m, watch, k, base) ||
                pokeCase(m, k, kVariPoke, base) < 0)
                return faulted(m);
        }
    int levels = 0, shifts = 0, variShifts = 0;
    if (!setToneValue(m, kPlaybackModeId, 0) || !checkLevelLaw(m, kCases[0], levels) ||
        !checkShiftLaw(m, kCases[0], shifts) || !checkShiftLaw(m, kVariCase, variShifts))
        return faulted(m);
    ok = ok && levels == 0 && shifts == 0 && variShifts == 0;
    if (!ok) std::printf("splitbytes: a control moved nothing or a law differs\n");
    return ok ? 0 : 1;
}

} // namespace vc2
