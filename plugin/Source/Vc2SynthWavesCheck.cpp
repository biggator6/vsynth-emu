// xt_vc2synth's waves (v362): the disk image's waves, loaded by rom.4's own loaders (vc2LoadWaves, v503), given both
// sides: the part through giveWaves (the store, the sets, the memory and v455's samples) and each of rom.4's machines
// the same bytes at their addresses. What the port reads past them is 0, the project's model (rom.2's band reads past a
// wave are given zeros, xtFwHookBandReads): rom.4 is given zeros there, narrowly, and each side counts what it read so,
// the counts held equal:
// - v363: a player's sample load past the waves, wherever it lands (unmapped memory, rom.4's own image or .bss, which an
//   XT's reads give as samples): skipped with its register 0 and counted, against the port's reads past its samples.
//   Its sample loads are the renders' (0x893c6000…0x893c8000) indexed halfword loads but those reading their own tables
//   just past them (kTableReach, their count printed). v362 mapped areas 1 to 3's unmapped pages as zeros for them; the
//   skip gives every one of them 0, so the pages went, and any other load past mapped memory faults, as before v362;
// - LITE's loop record read through a pointer that leaves mapped memory (a stale word on a voice reused across kinds,
//   0x893c859a): its loads through r14 (0x893c7f00…0x893c8700) given a zero record there, counted, against the port's
//   records at an address its waves don't hold.
// v363: the factory patches read from the same disk image for the notes part's cases on them (vc2SynthFactoryPatches).
// HARDWARE_FIRMWARE.md, "The part given the disk image's waves" and "Sample loads past the waves". XT_VC2SYNTH_MAIN
// only.
#ifdef XT_VC2SYNTH_MAIN
#include "Vc2SynthCheck.h"
#include "Vc2Synth.h"
#include "Vc2PatchFile.h"
#include "Vc2WaveBoot.h"
#include "XtFiles.h"
#include <vector>

namespace {
constexpr const char* kDiskImage = "V-SynthXTDiskImage.bin.0.bin";   // beside rom.4
constexpr const char* kPatches = "Internal.vc2/Patch";                // v363: the factory patches' folder
constexpr uint32_t kPhysical = 0x1fffffffu;
// [FW] v363, the sample loads' rule: the players' renders span 0x893c6000…0x893c8000; each of their indexed halfword
// loads (mov.w @(r0,Rm),Rn) is a sample load unless its address lies within kTableReach (0x100) past its own pc, a
// render's own table: 0x893c6402 reads +0x10, 0x893c6730 +0x10…+0x1c, 0x893c7d70 +0x20 and +0x24. The table reads'
// count is printed on a line of its own, fcheck's tolerance test of the rule
constexpr uint32_t kRendersFrom = 0x893c6000u, kRendersTo = 0x893c8000u;
constexpr uint32_t kLiteFrom = 0x893c7f00u, kLiteTo = 0x893c8700u;         // [FW] LITE's note-on, settings, update
constexpr uint32_t kZeroRecord = 0x8C1FE000u;   // in the RAM Vc2Fw::start maps, nothing else there
constexpr uint16_t kLoadMask = 0xf00f, kLoadHalf = 0x000d;   // [FW] mov.w @(r0,Rm),Rn
constexpr uint16_t kRecordLongMask = 0xf0f0, kRecordLong = 0x50e0;   // [FW] mov.l @(disp,r14),Rn
constexpr uint16_t kRecordHalfMask = 0xfff0, kRecordHalf = 0x85e0;   // [FW] mov.w @(disp,r14),r0
constexpr uint32_t kLong = 4, kHalf = 2;
constexpr uint32_t kTableReach = 0x100;   // the sample loads' rule, above

// v363: a render's sample load past the waves skipped with its register 0, counted
void farReadsCounted(Sh4& c, const Vc2WavePool& pool, Vc2SynthPast& p) {
    const uint32_t base = pool.words.base & kPhysical, bytes = (uint32_t)pool.memory.size();
    for (uint32_t pc = kRendersFrom; pc < kRendersTo; pc += kHalf) {
        const auto op = (uint16_t)c.rd16(pc);
        if ((op & kLoadMask) != kLoadHalf) continue;
        const uint32_t m = (op >> 4) & 0xfu, n = (op >> 8) & 0xfu;
        c.hook(pc, [m, n, pc, base, bytes, &p](Sh4& s) {
            const uint32_t at = s.r[0] + s.r[m];
            const bool table = (at & kPhysical) - (pc & kPhysical) < kTableReach;   // v363: the render's own table
            const bool past = !table && (at & kPhysical) - base >= bytes;
            p.romFar += past ? 1 : 0;
            p.romTables += table ? 1 : 0;
            if (!past) return;
            s.r[n] = 0;
            s.pc += kHalf;
        });
    }
}

// LITE's loads through r14 (its loop record) where the address leaves mapped memory: r14 the zero record, counted
void recordsGivenZeros(Sh4& c, long& count) {
    for (uint32_t pc = kLiteFrom; pc < kLiteTo; pc += kHalf) {
        const auto op = (uint16_t)c.rd16(pc);
        const bool isLong = (op & kRecordLongMask) == kRecordLong, isHalf = (op & kRecordHalfMask) == kRecordHalf;
        if (!isLong && !isHalf) continue;
        const uint32_t scale = isLong ? kLong : kHalf, disp = (op & 0xfu) * scale;
        c.hook(pc, [disp, scale, &count](Sh4& s) {
            if (s.ptr(s.r[14] + disp, scale) != nullptr) return;
            s.r[14] = kZeroRecord;
            ++count;
        });
    }
}

bool diskOpened(const char* rom4, XtDiskImage& disk) {
    const std::string r = rom4;
    return disk.open(r.substr(0, r.find_last_of('/') + 1) + kDiskImage);
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
    farReadsCounted(c, w.pool, past);
    recordsGivenZeros(c, past.romRecords);
}

void vc2SynthWavesGiven(Vc2Synth& s, const Vc2LoadedWaves& w, Vc2SynthPast& past) {
    s.giveWaves(w);
    s.samples.far = &past.portFar;
}

bool vc2SynthWavesBoth(const char* rom4, Vc2Fw& fw, Vc2Synth& s, Vc2LoadedWaves& w, Vc2SynthPast& past) {
    XtDiskImage disk;
    std::string why = "no disk image beside rom.4";
    const bool loaded = diskOpened(rom4, disk) && vc2LoadWaves(rom4, Vc2DiskBootFiles(disk), w, &why);
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

bool vc2SynthFactoryPatches(const char* rom4, std::vector<std::vector<uint8_t>>& out) {
    XtDiskImage disk;
    if (!vc2LoadPatchFiles(rom4) || !diskOpened(rom4, disk)) {
        std::printf("xt_vc2synth: the factory patches: no .p02 reader or disk image\n");
        return false;
    }
    for (const XtDiskImage::Entry& e : disk.list(kPatches)) {
        const std::vector<uint8_t> bytes = disk.read(e);
        Vc2PatchFile f;
        if (!e.isDir && vc2PatchRead(bytes.data(), bytes.size(), f)) out.push_back(f.store);
    }
    std::printf("synth: the disk image's %zu factory patches\n", out.size());
    return !out.empty();
}

void vc2SynthPortRecords(const Vc2Synth& s, Vc2SynthPast& past) {
    for (const Vc2SynthTone& h : s.hosts) past.portRecords += h.recordsPast();
}

long vc2SynthPastDifference(const Vc2SynthPast& past) {
    std::printf("synth waves: the reads past them, given zeros: far reads rom.4 %ld, the port %ld; records past rom.4 "
                "%ld, the port %ld\n",
                past.romFar, past.portFar, past.romRecords, past.portRecords);
    std::printf("synth waves: the renders' own table reads %ld (v363: the sample loads' rule)\n", past.romTables);
    return past.romFar != past.portFar || past.romRecords != past.portRecords ? 1 : 0;
}
#endif
