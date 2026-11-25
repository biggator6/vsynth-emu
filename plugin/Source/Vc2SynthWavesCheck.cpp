// xt_vc2synth's waves (v362): the disk image's waves, loaded by rom.4's own loaders (vc2LoadWaves, v503), given both
// sides: the part through giveWaves (the store, the sets, the memory and v455's samples) and each of rom.4's machines
// the same bytes at their addresses. What the port reads past them is 0, the project's model (rom.2's band reads past a
// wave are given zeros, xtFwHookBandReads): rom.4 is given zeros there, narrowly, and each side counts what it read so,
// the counts held equal:
// - a player's sample read past the waves: areas 1 to 3's pages nothing maps become zeros (area 0, where an XT's boot
//   flash lies, stays unmapped, so a read there still faults); rom.4's halfword loads in the players' renders
//   (0x893c6000…0x893c8000) that land in those pages counted, against the port's reads past its samples;
// - LITE's loop record read through a pointer that leaves mapped memory (a stale word on a voice reused across kinds,
//   0x893c859a): its loads through r14 (0x893c7f00…0x893c8700) given a zero record there, counted, against the port's
//   records at an address its waves don't hold.
// HARDWARE_FIRMWARE.md, "The part given the disk image's waves". XT_VC2SYNTH_MAIN only.
#ifdef XT_VC2SYNTH_MAIN
#include "Vc2SynthCheck.h"
#include "Vc2Synth.h"
#include "Vc2WaveBoot.h"
#include "XtFiles.h"
#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

namespace {
constexpr const char* kDiskImage = "V-SynthXTDiskImage.bin.0.bin";   // beside rom.4
constexpr uint32_t kZerosFrom = 0x84000000u, kZerosTo = 0x90000000u, kPage = 0x1000u, kPhysical = 0x1fffffffu;
constexpr uint32_t kRendersFrom = 0x893c6000u, kRendersTo = 0x893c8000u;   // [FW] the players' renders
constexpr uint32_t kLiteFrom = 0x893c7f00u, kLiteTo = 0x893c8700u;         // [FW] LITE's note-on, settings, update
constexpr uint32_t kZeroRecord = 0x8C1FE000u;   // in the RAM Vc2Fw::start maps, nothing else there
constexpr uint16_t kLoadMask = 0xf00f, kLoadHalf = 0x000d;   // [FW] mov.w @(r0,Rm),Rn
constexpr uint16_t kRecordLongMask = 0xf0f0, kRecordLong = 0x50e0;   // [FW] mov.l @(disp,r14),Rn
constexpr uint16_t kRecordHalfMask = 0xfff0, kRecordHalf = 0x85e0;   // [FW] mov.w @(disp,r14),r0
constexpr uint32_t kLong = 4, kHalf = 2;

// A machine's zero pages, by their physical runs
using Zeros = std::vector<std::pair<uint32_t, uint32_t>>;

bool inZeros(const Zeros& zeros, uint32_t at) {
    const uint32_t phys = at & kPhysical;
    return std::any_of(zeros.begin(), zeros.end(), [phys](const auto& run) { return phys - run.first < run.second; });
}

// Areas 1 to 3's pages that nothing maps, mapped as zeros
std::shared_ptr<const Zeros> zerosMapped(Sh4& c) {
    auto zeros = std::make_shared<Zeros>();
    for (uint32_t at = kZerosFrom; at < kZerosTo;) {
        if (c.ptr(at, kLong) != nullptr) {
            at += kPage;
            continue;
        }
        uint32_t end = at;
        while (end < kZerosTo && c.ptr(end, kLong) == nullptr) end += kPage;
        c.mapRam(at, end - at);
        zeros->push_back({ at & kPhysical, end - at });
        at = end;
    }
    return zeros;
}

// rom.4's renders' halfword loads landing in the zero pages, counted
void farReadsCounted(Sh4& c, const std::shared_ptr<const Zeros>& zeros, long& count) {
    for (uint32_t pc = kRendersFrom; pc < kRendersTo; pc += kHalf) {
        const auto op = (uint16_t)c.rd16(pc);
        if ((op & kLoadMask) != kLoadHalf) continue;
        const uint32_t m = (op >> 4) & 0xfu;
        c.hook(pc, [m, zeros, &count](Sh4& s) { count += inZeros(*zeros, s.r[0] + s.r[m]) ? 1 : 0; });
    }
}

// LITE's loads through r14 (its loop record) where the address leaves mapped memory: r14 the zero record, counted
void recordsGivenZeros(Sh4& c, const std::shared_ptr<const Zeros>& zeros, long& count) {
    for (uint32_t pc = kLiteFrom; pc < kLiteTo; pc += kHalf) {
        const auto op = (uint16_t)c.rd16(pc);
        const bool isLong = (op & kRecordLongMask) == kRecordLong, isHalf = (op & kRecordHalfMask) == kRecordHalf;
        if (!isLong && !isHalf) continue;
        const uint32_t scale = isLong ? kLong : kHalf, disp = (op & 0xfu) * scale;
        c.hook(pc, [disp, scale, zeros, &count](Sh4& s) {
            const uint32_t at = s.r[14] + disp;
            if (s.ptr(at, scale) != nullptr && !inZeros(*zeros, at)) return;
            s.r[14] = kZeroRecord;
            ++count;
        });
    }
}
} // namespace

void vc2SynthWavesWritten(Sh4& c, const Vc2LoadedWaves& w, Vc2SynthPast& past) {
    c.write(Vc2WaveStore::kAt, &w.store.words, sizeof w.store.words);
    c.write(Vc2WavePool::kAt, &w.pool.words, sizeof w.pool.words);
    c.write(Vc2WaveSets::kCountsAt, &w.sets.counts, sizeof w.sets.counts);
    c.write(Vc2WaveSets::kTablesAt, &w.sets.tables, sizeof w.sets.tables);
    c.write(Vc2WaveSets::kBuffersAt, &w.sets.buffers, sizeof w.sets.buffers);
    c.write(Vc2WaveSets::kVelocityDataAt, w.sets.data.velocity.data(), w.sets.data.velocity.size());
    c.write(Vc2WaveSets::kKeyDataAt, w.sets.data.key.data(), w.sets.data.key.size());
    if (!w.pool.memory.empty() && c.ptr(w.pool.words.base, kLong) == nullptr)
        c.mapRam(w.pool.words.base, (uint32_t)w.pool.memory.size());
    if (!w.pool.memory.empty()) c.write(w.pool.words.base, w.pool.memory.data(), w.pool.memory.size());
    const auto zeros = zerosMapped(c);
    farReadsCounted(c, zeros, past.romFar);
    recordsGivenZeros(c, zeros, past.romRecords);
}

void vc2SynthWavesGiven(Vc2Synth& s, const Vc2LoadedWaves& w, Vc2SynthPast& past) {
    s.giveWaves(w);
    s.samples.far = &past.portFar;
}

bool vc2SynthWavesBoth(const char* rom4, Vc2Fw& fw, Vc2Synth& s, Vc2LoadedWaves& w, Vc2SynthPast& past) {
    const std::string r = rom4;
    XtDiskImage disk;
    std::string why = "no disk image beside rom.4";
    const bool loaded = disk.open(r.substr(0, r.find_last_of('/') + 1) + kDiskImage) &&
                        vc2LoadWaves(r, Vc2DiskBootFiles(disk), w, &why);
    if (!loaded) {
        std::printf("xt_vc2synth: the waves: %s\n", why.c_str());
        return false;
    }
    vc2SynthWavesWritten(fw.cpu, w, past);
    vc2SynthWavesGiven(s, w, past);
    std::printf("synth: the disk image's waves on both sides, the pool's %zu bytes at %08x\n", w.pool.memory.size(),
                w.pool.words.base);
    return true;
}

void vc2SynthPortRecords(const Vc2Synth& s, Vc2SynthPast& past) {
    for (const Vc2SynthTone& h : s.hosts) past.portRecords += h.recordsPast();
}

long vc2SynthPastDifference(const Vc2SynthPast& past) {
    std::printf("synth waves: the reads past them, given zeros: far reads rom.4 %ld, the port %ld; records past rom.4 "
                "%ld, the port %ld\n",
                past.romFar, past.portFar, past.romRecords, past.portRecords);
    return past.romFar != past.portFar || past.romRecords != past.portRecords ? 1 : 0;
}
#endif
