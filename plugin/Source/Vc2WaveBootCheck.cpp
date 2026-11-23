// xt_vc2waves' fourth part (v503): the VC-2's waves loaded by rom.4 itself, Vc2WaveBoot on the disk image's files.  The
// load must run every stage, and what it leaves must be what the files say: the sets as the port's parse (v500) of the
// same index makes them, a library record for each link file of A:\Internal.vc2\Wave naming the wave and the name its
// "link" and "name" chunks give, and every sample the index names loaded, its vpsc, vpno, vplp and vpdt as its stream
// in the bank has them and named by its file.  What the port takes must be rom.4's memory byte for byte, by the
// check's own addresses.  Then the load again with every one of rom.4's 382 static initializers run first (the kernel
// calls the load doesn't answer given 0 here) and two decoys in the Wave folder that its filters must leave out: the
// same memory byte for byte, so the three the load runs are all its loaders need.  Last the port's lookups on what it
// took against rom.4's own (Vc2WaveLookupCheck.cpp).  XT_VC2WAVES_MAIN only.
#ifdef XT_VC2WAVES_MAIN
#include "Vc2WaveIndex.h"
#include "Vc2WavesCheck.h"
#include <algorithm>
#include <charconv>
#include <cstring>

using xtfile::le32;

namespace {
// [FW] where the loaders leave what the port takes: the store's words, the pool's, the sets' counts, tables, buffers,
// the velocity's and the key's data; the pool's memory past its size that its size law can give it
struct Region {
    uint32_t at, bytes;
    const char* what;
};
constexpr Region kRegions[] = {
    { 0x8997c52c, 0x204, "the store's words" },        { 0x8997c51c, 0x10, "the pool's words" },
    { 0x8993448c, 6, "the sets' counts" },             { 0x89973084, 0x2710, "the sets' tables" },
    { 0x89989754, 0x524, "the sets' buffers" },        { 0x89d65e38, 500 * 0x210, "the velocity's data" },
    { 0x89da6578, 500 * 0x310, "the key's data" },
};
constexpr uint32_t kPoolWordsAt = 0x8997c51c, kOverhang = 24;
// [FW] the static initializers' table, a row 12 bytes, its routine first
constexpr uint32_t kRowsFrom = 0x898c59c4, kRowsTo = 0x898c6ba0, kRowBytes = 12;
constexpr uint32_t kKernelFrom = 0x88000000, kKernelTo = 0x89000000;
// [FW] a record's chunks: its slot and the chunk's name in a stream; a chunk's data 28 bytes in, its length at +0x10
struct ChunkRow {
    uint32_t slot;
    const char* name;
};
constexpr ChunkRow kChunks[] = { { 0x28, "vpsc" }, { 0x2c, "vpno" }, { 0x30, "vplp" }, { 0x34, "vpdt" } };
constexpr uint32_t kChunkData = 28, kChunkLength = 0x10;
// [COR] a link file: "link", 4, the wave's number, "name", 16, the name (wlflib.py); a library record's key 2999 +
// the number its name starts with
constexpr size_t kLinkNumberAt = 8, kLinkNameAt = 20, kLinkBytes = 36;
constexpr uint32_t kLibraryKeys = 2999;
constexpr const char* kWaveFolder = "A:\\Internal.vc2\\Wave\\";
// [FW] a record's name, at most 12 characters from +0x58: a sample's its file's name's first, a library record's its
// link file's name's
constexpr uint32_t kNameAt = 0x58;
constexpr size_t kNameMost = 12;
// [COR] the index's sections, 16 bytes from +0x40 (the number, a flag, the place); the samples' (3): a count, where
// its entries are, an entry's size, each a 32-byte path; the bank's '"TABLE"', count, 60-byte entries, '"DATA"'
constexpr uint32_t kSamples = 3, kPathBytes = 32;
constexpr size_t kCountAt = 7, kBankEntries = 11, kBankEntry = 60, kBankData = 6;

std::string said(const char* what, long n) { return std::string(what) + " " + std::to_string(n); }

// The whole number of the digits a text starts with; false when it starts with none
bool digitsAt(const char* from, const char* to, uint32_t& n) { return std::from_chars(from, to, n).ptr != from; }

// A record's name as rom.4 left it, and the first characters of a name as the reader keeps them
std::string recordName(const Vc2LoadedWaves& w, uint32_t record) {
    std::string s;
    for (uint32_t k = 0; k < kNameMost; ++k) {
        const char ch = (char)w.byte(record + kNameAt + k);
        if (ch == 0) break;
        s += ch;
    }
    return s;
}

std::string keptName(const std::string& name) { return name.substr(0, std::min(name.find('\0'), kNameMost)); }

// The memory the port took against rom.4's (`c`)
std::string takenDifference(Sh4& c, const Vc2LoadedWaves& w) {
    const void* taken[] = { &w.store.words, &w.pool.words, &w.sets.counts, &w.sets.tables, &w.sets.buffers,
                            w.sets.data.velocity.data(), w.sets.data.key.data() };
    for (size_t k = 0; k < std::size(kRegions); ++k)
        if (std::string d = vc2BytesDifference(c, kRegions[k].at, taken[k], kRegions[k].bytes, kRegions[k].what);
            !d.empty())
            return d;
    const uint32_t base = c.rd32(kPoolWordsAt), size = c.rd32(kPoolWordsAt + 8);
    if (w.pool.memory.size() != size + kOverhang) return "the pool's memory's size";
    return vc2BytesDifference(c, base, w.pool.memory.data(), w.pool.memory.size(), "the pool's memory");
}

// The sets against the port's parse of the same index (v500): counts, tables and data
std::string indexSetsDifference(const Vc2LoadedWaves& w, const std::vector<uint8_t>& index) {
    Vc2FileList list;
    list.files.push_back({ Vc2DiskBootFiles::kIndexPath, index });
    Vc2WaveIndex parse;
    Vc2WaveIndexState state;
    Vc2WaveSets sets;
    parse.parse(state, list, sets);
    if (std::memcmp(&sets.counts, &w.sets.counts, sizeof sets.counts)) return "the sets' counts";
    if (std::memcmp(&sets.tables, &w.sets.tables, sizeof sets.tables)) return "the sets' tables";
    return sets.data.velocity == w.sets.data.velocity && sets.data.key == w.sets.data.key ? "" : "the sets' data";
}

// Each link file's library record names the wave its "link" chunk names
std::string libraryDifference(const Vc2LoadedWaves& w, const Vc2BootFiles& files, long& links) {
    std::vector<Vc2BootEntry> entries;
    files.folder(kWaveFolder, entries);
    for (const Vc2BootEntry& e : entries) {
        std::vector<uint8_t> bytes;
        uint32_t number = 0;
        if (!digitsAt(e.name.data(), e.name.data() + e.name.size(), number)) continue;
        if (!files.file(kWaveFolder + e.name, bytes) || bytes.size() < kLinkBytes) continue;
        if (std::memcmp(bytes.data(), "link", 4)) continue;
        ++links;
        const uint32_t key = kLibraryKeys + number;
        if (w.store.setIndex(key, w) != (int32_t)le32(&bytes[kLinkNumberAt])) return "the library's " + e.name;
        const std::string name(bytes.begin() + kLinkNameAt, bytes.begin() + kLinkBytes);
        if (recordName(w, w.store.recordOf(key, w)) != keptName(name)) return "the library's name of " + e.name;
    }
    return "";
}

// The index's sample paths by number from 1 (entry 0 none)
std::vector<std::string> samplePaths(const std::vector<uint8_t>& ix) {
    std::vector<std::string> out(1);
    for (uint32_t s = 0; ix.size() >= 0x40 && s < le32(&ix[8]); ++s) {
        const uint8_t* e = &ix[0x40 + 16 * s];
        if ((e[0] | e[1] << 8) != kSamples) continue;
        const uint32_t at = le32(e + 4), count = le32(&ix[at]) & 0xffff, from = at + le32(&ix[at + 4]);
        for (uint32_t k = 0; k < count; ++k) {
            const char* p = reinterpret_cast<const char*>(&ix[from + le32(&ix[at + 8]) * k]);
            out.emplace_back(p, strnlen(p, kPathBytes));
        }
    }
    return out;
}

// A stream's chunk `name` in the bank by its file's name (a path's last part), as its bytes; false when there is none
bool bankChunk(const std::vector<uint8_t>& bank, const std::string& path, const char* name, std::vector<uint8_t>& out) {
    const std::string file = path.substr(path.find_last_of('\\') + 1);
    uint32_t count = 0;
    const auto* digits = reinterpret_cast<const char*>(bank.data());
    if (!digitsAt(digits + kCountAt, digits + kBankEntries, count)) return false;
    const size_t base = kBankEntries + kBankEntry * count + kBankData;
    for (uint32_t k = 0; k < count; ++k) {
        const uint8_t* e = &bank[kBankEntries + kBankEntry * k];
        if (file != reinterpret_cast<const char*>(e)) continue;
        const size_t from = base + le32(e + 56), end = from + le32(e + 52);
        for (size_t at = from; at + 8 <= end; at += 8 + le32(&bank[at + 4]) + (le32(&bank[at + 4]) & 1)) {
            if (std::memcmp(&bank[at], name, 4)) continue;
            out.assign(bank.begin() + (long)at + 8, bank.begin() + (long)(at + 8 + le32(&bank[at + 4])));
            return true;
        }
    }
    return false;
}

// Every sample loaded, its four chunks as its stream has them
std::string samplesDifference(const Vc2LoadedWaves& w, const Vc2DiskBootFiles& files, long& samples) {
    const std::vector<std::string> paths = samplePaths(files.index());
    for (uint32_t n = 1; n < paths.size(); ++n, ++samples) {
        const uint32_t record = w.store.loadedRecord(n, w);
        if (record == 0) return said("sample", n) + " isn't loaded";
        for (const ChunkRow& r : kChunks) {
            std::vector<uint8_t> theirs;
            const uint32_t chunk = w.pool.word(record + r.slot);
            const bool there = bankChunk(files.bank(), paths[n], r.name, theirs);
            std::vector<uint8_t> mine(chunk == 0 ? 0 : w.pool.word(chunk + kChunkLength));
            for (uint32_t i = 0; i < mine.size(); ++i) mine[i] = w.pool.byte(chunk + kChunkData + i);
            if (there != (chunk != 0) || mine != theirs) return said("sample", n) + "'s " + r.name;
        }
        if (recordName(w, record) != keptName(paths[n].substr(paths[n].find_last_of('\\') + 1)))
            return said("sample", n) + "'s name";
    }
    return "";
}

// The load again on a machine where every static initializer ran first, the kernel the load doesn't answer given 0
// The disk image's files with two decoys in the Wave folder that the loaders' filters leave out when the kernel's
// compares work: a link file naming wave 1 as Wave Number 90 (the link files take 1…58) but a ".w03", and a ".wa2", a
// ".wav" to a compare one character short.  Taken, either would leave a record or a fault the plain load hasn't.
class DecoyFiles : public Vc2BootFiles {
public:
    explicit DecoyFiles(const Vc2BootFiles& real) : real_(real) {
        const uint8_t link[] = { 'l', 'i', 'n', 'k', 4, 0, 0, 0, 1, 0, 0, 0, 'n', 'a', 'm', 'e', 16, 0, 0, 0 };
        decoy_.assign(std::begin(link), std::end(link));
        const std::string name = "Decoy           ";
        decoy_.insert(decoy_.end(), name.begin(), name.end());
    }
    bool file(const std::string& path, std::vector<uint8_t>& out) const override {
        for (const char* d : kDecoys) {
            if (path != std::string(kWaveFolder) + d) continue;
            out = decoy_;
            return true;
        }
        return real_.file(path, out);
    }
    bool folder(const std::string& path, std::vector<Vc2BootEntry>& out) const override {
        if (!real_.folder(path, out)) return false;
        if (path == kWaveFolder)
            for (const char* d : kDecoys) out.push_back({ d, {}, kArchive, (uint32_t)decoy_.size() });
        return true;
    }

private:
    static constexpr const char* kDecoys[] = { "090 Decoy.w03", "091 Decoy.wa2" };
    static constexpr uint8_t kArchive = 0x20;
    const Vc2BootFiles& real_;
    std::vector<uint8_t> decoy_;
};

std::string everyInitializer(const std::string& rom4, const Vc2BootFiles& files, Sh4& first, long& rows) {
    const DecoyFiles decoys(files);
    Vc2WaveBoot again(rom4, decoys);
    std::string why;
    if (!again.start(&why)) return why;
    Sh4& c = again.cpu();
    for (uint32_t pc = 0x89300000; pc + 2 < c.image().dataFrom; pc += 2) {
        if (c.rd16(pc) >> 12 != 0xd) continue;
        const uint32_t literal = c.rd32(((pc + 4) & ~3u) + 4 * (c.rd16(pc) & 0xff));
        if (literal >= kKernelFrom && literal < kKernelTo && !Vc2WaveBoot::answers(literal))
            c.hook(literal, [](Sh4& s) { s.ret(0); });
    }
    for (uint32_t row = kRowsFrom; row <= kRowsTo; row += kRowBytes, ++rows)
        if (!c.call(c.rd32(row), {}, Vc2WaveBoot::kStack, nullptr, 2'000'000'000ull)) return said("row", row);
    if (!again.load(&why)) return why;
    Vc2LoadedWaves w;
    again.take(w);
    return takenDifference(first, w);
}
} // namespace

long vc2WaveBootPart(const std::string& rom4, const Vc2Run& run, const Vc2DiskBootFiles& files) {
    Vc2WavesTally tally { "the load" };
    Vc2WaveBoot boot(rom4, files);
    std::string why;
    const bool loaded = boot.start(&why) && boot.initialize(&why) && boot.load(&why);
    tally.outcome(loaded ? "" : why, "the load");
    if (!loaded) return tally.say("loads, none run");
    Vc2LoadedWaves w;
    boot.take(w);
    const uint64_t steps = boot.cpu().steps();
    long links = 0, samples = 0, rows = 0;
    tally.outcome(indexSetsDifference(w, files.index()), "the sets");
    tally.outcome(libraryDifference(w, files, links), "the library");
    tally.outcome(samplesDifference(w, files, samples), "the samples");
    tally.outcome(takenDifference(boot.cpu(), w), "what the port took");
    tally.outcome(everyInitializer(rom4, files, boot.cpu(), rows), "every initializer");   // before a lookup writes
    const long lookups = vc2WaveLookups(boot.cpu(), w, run, tally);
    return tally.say("checks: " + std::to_string(steps) + " steps; sets " +
                     std::to_string(w.sets.counts.velocity) + ", " + std::to_string(w.sets.counts.key) + ", " +
                     std::to_string(w.sets.counts.alias) + "; " + std::to_string(links) + " link files; " +
                     std::to_string(samples) + " samples; " + std::to_string(lookups) + " lookups; " +
                     std::to_string(rows) + " initializers first");
}
#endif
