// xt_vc2waves' index part (v500): rom.4's parse 0x8934d120 against Vc2WaveIndex::parse, each on its own copy of the
// same file and the same memory: the parse's state, its return, every file call, and after it the progress, the
// reader's words, the sets' counts and tables, and the velocity's and key's data.  First the disk image's index on the
// sets as C startup leaves them; then cases of drawn indexes, one to three parses each, the sets carried from one to
// the next, starting from C startup's or from noise.  The draws: a WLF0 file of up to four sections, each a number 1…3
// mostly (now and then 0, 4 or 0xffff) and its second halfword 1 mostly; a set's section of 1…40 records mostly (now
// and then none, or 498…505 across the table's 500), the count's upper halfword noise a time in eight, index entries
// of 8 bytes mostly, records the slot's size half the time, any less, or now and then run on through the next buffers
// to the reader's words' end; an alias's section of 36-byte entries mostly (32…38), their types 0, 1, 3, 5 or any and
// links 0 or about the count; now and then the last record cut short by the file's end, a magic byte wrong, or no
// file.  Not drawn: a read onto rom.4's stack that the file cuts short (a header, an index entry or a section's entry),
// which leaves the stack's own words.  Each side's file calls carry the progress as the call is made, which the XT's
// display reads while a parse runs (the steps between sections show nowhere else).  XT_VC2WAVES_MAIN only.
#ifdef XT_VC2WAVES_MAIN
#include "Vc2WaveIndex.h"
#include "Vc2WavesCheck.h"
#include <cstddef>
#include <cstring>
#include <map>

namespace {
constexpr uint32_t kParse = 0x8934d120;      // [FW]
constexpr uint32_t kStateAt = 0x8C020000;    // the parse's state, in the check's RAM
constexpr const char* kIndexPath = "D:\\VC-2 Wave.bin";
constexpr const char* kElsewhere = "D:\\VC-2 Wave.old";   // an index that isn't where the parse looks
constexpr uint32_t kHeader = 12, kSectionsAt = 0x40, kSectionBytes = 16, kVersionsAt = 16, kVersionsBytes = 32;
constexpr uint32_t kAliasAt = 32;
constexpr uint32_t kVelocityRoom = sizeof(Vc2WaveIndexWords) - offsetof(Vc2WaveIndexWords, velocityRead);
constexpr uint32_t kKeyRoom = sizeof(Vc2WaveIndexWords) - offsetof(Vc2WaveIndexWords, keyRead);

// The index's file as it is drawn: its bytes, words put little-endian, and whether its last bytes are a record's or an
// entry's that a read takes into the reader's words (so the file may end inside them)
struct FileDraw {
    std::mt19937& rng;
    std::vector<uint8_t> bytes;
    bool endsInRecord = false;
    size_t lastRecord = 0;
    size_t append(size_t n) {
        const size_t at = bytes.size();
        for (size_t i = 0; i < n; ++i) bytes.push_back(static_cast<uint8_t>(rng()));
        endsInRecord = false;
        return at;
    }
    size_t record(size_t n) {
        lastRecord = append(n);
        endsInRecord = n > 0;
        return lastRecord;
    }
    void word(size_t at, uint32_t v) {
        for (int i = 0; i < 4; ++i) bytes[at + static_cast<size_t>(i)] = static_cast<uint8_t>(v >> (8 * i));
    }
    void half(size_t at, uint16_t v) {
        bytes[at] = static_cast<uint8_t>(v);
        bytes[at + 1] = static_cast<uint8_t>(v >> 8);
    }
};

uint32_t drawEntries(std::mt19937& rng) {
    if (rng() % 8 == 0) return 0;
    if (rng() % 12 == 0) return 498 + rng() % 8;
    return 1 + rng() % 40;
}

// A section's header: its count (the upper halfword noise a time in eight), where its index is, an entry's size
void sectionHeader(FileDraw& f, size_t at, uint32_t entries, uint32_t indexAt, uint32_t entryBytes) {
    f.word(at, (f.rng() % 8 == 0 ? f.rng() << 16 : 0) | entries);
    f.word(at + 4, indexAt);
    f.word(at + 8, entryBytes);
}

// A velocity's or key's section: its index entries, then its records
size_t setBody(FileDraw& f, uint32_t slot, uint32_t room) {
    const uint32_t entries = drawEntries(f.rng), entryBytes = f.rng() % 8 ? 8 : 8 + 4 * (1 + f.rng() % 2);
    const uint32_t indexAt = kHeader + 4 * (f.rng() % 4);
    const size_t at = f.append(indexAt + entries * entryBytes);
    sectionHeader(f, at, entries, indexAt, entryBytes);
    for (uint32_t n = 0; n < entries; ++n) {
        const uint32_t pick = f.rng() % 16;
        const uint32_t size = pick < 8 ? slot : pick < 15 ? f.rng() % (slot + 1) : slot + f.rng() % (room - slot + 1);
        const size_t record = f.record(size);
        f.word(at + indexAt + n * entryBytes, static_cast<uint32_t>(record - at));
        f.word(at + indexAt + n * entryBytes + 4, size);
    }
    return at;
}

// An alias's section: its entries one after another, each a path's noise and its four bytes: a type, a byte, a link
size_t aliasBody(FileDraw& f) {
    const uint32_t entries = drawEntries(f.rng), entryBytes = f.rng() % 4 ? 36 : 32 + f.rng() % 7;
    const uint32_t indexAt = kHeader + 4 * (f.rng() % 4);
    const size_t at = f.append(indexAt);
    sectionHeader(f, at, entries, indexAt, entryBytes);
    constexpr uint8_t kTypes[] = { 0, 1, 3, 5 };
    for (uint32_t n = 0; n < entries; ++n) {
        const size_t entry = f.record(entryBytes);
        if (entryBytes <= kAliasAt + 3) continue;
        f.bytes[entry + kAliasAt] = f.rng() % 5 ? kTypes[f.rng() % 4] : static_cast<uint8_t>(f.rng());
        f.half(entry + kAliasAt + 2, static_cast<uint16_t>(f.rng() % 2 ? 0 : f.rng() % (entries + 2)));
    }
    return at;
}

std::vector<uint8_t> drawIndex(std::mt19937& rng) {
    FileDraw f { rng };
    f.append(kSectionsAt);
    const char magic[] = "WLF08";
    std::copy_n(magic, 5, f.bytes.begin());
    if (rng() % 16 == 0) f.bytes[rng() % 4] ^= static_cast<uint8_t>(1 + rng() % 255);
    for (uint32_t i = 0; i < kVersionsBytes; ++i) f.bytes[kVersionsAt + i] = static_cast<uint8_t>(' ' + rng() % 95);
    const uint32_t sections = rng() % 5;
    f.word(8, sections);
    f.append(kSectionBytes * sections);
    constexpr uint16_t kNumbers[] = { 1, 2, 3, 0, 4, 0xffff };
    for (uint32_t i = 0; i < sections; ++i) {
        const uint16_t number = kNumbers[rng() % 8 ? rng() % 3 : 3 + rng() % 3];
        const uint16_t on = rng() % 8 ? 1 : static_cast<uint16_t>(rng() % 3 == 0 ? 0 : 2 + rng() % 0x8000);
        const size_t body = number == 1   ? setBody(f, kVc2VelocitySlot, kVelocityRoom)
                            : number == 2 ? setBody(f, kVc2KeySlot, kKeyRoom)
                            : number == 3 ? aliasBody(f)
                                          : rng() % f.bytes.size();
        const size_t entry = kSectionsAt + kSectionBytes * i;
        f.half(entry, number);
        f.half(entry + 2, on);
        f.word(entry + 4, static_cast<uint32_t>(body));
        f.word(entry + 8, static_cast<uint32_t>(f.bytes.size() - body));
    }
    if (f.endsInRecord && rng() % 8 == 0) f.bytes.resize(f.lastRecord + rng() % (f.bytes.size() - f.lastRecord));
    return f.bytes;
}

// The port's side: the parse's memory and state, and what its parses gave
struct PortSide {
    Vc2WaveIndex index;
    Vc2WaveSets sets;
    Vc2WaveIndexState state;
    std::map<int32_t, long>& gave;
};

// Both sides' memory as the port's: the progress, the reader's words, the sets and the parse's state
void putPort(Vc2Fw& fw, const PortSide& p) {
    fw.cpu.write(Vc2WaveIndex::kProgressAt, &p.index.progress, sizeof p.index.progress);
    fw.cpu.write(Vc2WaveIndex::kWordsAt, &p.index.words, sizeof p.index.words);
    fw.cpu.write(Vc2WaveSets::kCountsAt, &p.sets.counts, sizeof p.sets.counts);
    fw.cpu.write(Vc2WaveSets::kTablesAt, &p.sets.tables, sizeof p.sets.tables);
    fw.cpu.write(Vc2WaveSets::kVelocityDataAt, p.sets.data.velocity.data(), p.sets.data.velocity.size());
    fw.cpu.write(Vc2WaveSets::kKeyDataAt, p.sets.data.key.data(), p.sets.data.key.size());
    fw.cpu.write(kStateAt, &p.state, sizeof p.state);
}

// Each stream the parse made, read by its address through the port's dataAt (as a wave memory reads it) against rom.4's
// memory there: the velocity's and key's entries whose stream is their own slot; and no byte just outside the data
// (the key's follows the velocity's)
std::string streamsDifference(Vc2Fw& fw, const Vc2WaveSets& sets) {
    if (sets.dataAt(Vc2WaveSets::kVelocityDataAt - 1) || sets.dataAt(Vc2WaveSets::kKeyDataAt + sets.data.key.size()))
        return "a byte by address outside the sets' data";
    const std::tuple<const std::array<uint32_t, 2 * kVc2WaveSetEntries>*, uint32_t, uint32_t, const char*> rows[] = {
        { &sets.tables.velocity, Vc2WaveSets::kVelocityDataAt, kVc2VelocitySlot, "the velocity's stream" },
        { &sets.tables.key, Vc2WaveSets::kKeyDataAt, kVc2KeySlot, "the key's stream" },
    };
    for (const auto& [table, dataAt, slot, what] : rows) {
        std::vector<uint8_t> theirs(slot);
        for (uint32_t n = 0; n < kVc2WaveSetEntries; ++n) {
            const uint32_t at = (*table)[2 * n];
            if (at != dataAt + slot * n || (*table)[2 * n + 1] != slot) continue;
            fw.cpu.read(at, theirs.data(), slot);
            for (uint32_t i = 0; i < slot; ++i)
                if (const uint8_t* mine = sets.dataAt(at + i); !mine || *mine != theirs[i])
                    return std::string(what) + " " + std::to_string(n + 1) + ", byte " + std::to_string(i);
        }
    }
    return "";
}

std::string memoryDifference(Vc2Fw& fw, const PortSide& p) {
    const Vc2WaveSetData& data = p.sets.data;
    const std::tuple<uint32_t, const void*, size_t, const char*> regions[] = {
        { kStateAt, &p.state, sizeof p.state, "the state" },
        { Vc2WaveIndex::kProgressAt, &p.index.progress, sizeof p.index.progress, "the progress" },
        { Vc2WaveIndex::kWordsAt, &p.index.words, sizeof p.index.words, "the reader's words" },
        { Vc2WaveSets::kCountsAt, &p.sets.counts, sizeof p.sets.counts, "the counts" },
        { Vc2WaveSets::kTablesAt, &p.sets.tables, sizeof p.sets.tables, "the tables" },
        { Vc2WaveSets::kVelocityDataAt, data.velocity.data(), data.velocity.size(), "the velocity's data" },
        { Vc2WaveSets::kKeyDataAt, data.key.data(), data.key.size(), "the key's data" },
    };
    for (const auto& [at, mine, bytes, what] : regions)
        if (std::string d = vc2BytesDifference(fw.cpu, at, mine, bytes, what); !d.empty()) return d;
    return streamsDifference(fw, p.sets);
}

// A parse on both sides of `file` at `path`, from the port's memory, each side's file calls watching its progress;
// what differs, or nothing
std::string parseBoth(Vc2Fw& fw, PortSide& p, const std::vector<uint8_t>& file, const char* path) {
    putPort(fw, p);
    Vc2FileList theirs, mine;
    theirs.files.push_back({ path, file });
    mine.files.push_back({ path, file });
    mine.watch = [&p] {
        uint64_t seen = 0;
        std::memcpy(&seen, &p.index.progress, sizeof p.index.progress);
        return seen;
    };
    theirs.watch = [&fw] {
        uint64_t seen = 0;
        fw.cpu.read(Vc2WaveIndex::kProgressAt, &seen, sizeof(Vc2WaveLoadProgress));
        return seen;
    };
    vc2HookFiles(fw, theirs);
    uint32_t r0 = 0;
    if (!fw.cpu.call(kParse, { kStateAt }, kVc2Sp, &r0, kVc2LoadSteps)) return "the parse faults: " + fw.cpu.error();
    const int32_t got = p.index.parse(p.state, mine, p.sets);
    ++p.gave[got];
    if (static_cast<int32_t>(r0) != got)
        return "the parse gives " + std::to_string(got) + ", rom.4's " + std::to_string(static_cast<int32_t>(r0));
    std::string d = vc2FileCallsDifference(mine.calls, theirs.calls);
    return d.empty() ? memoryDifference(fw, p) : d;
}

// The port's memory as noise: the progress, the words, the sets and the state, as any earlier run may leave them
void noise(std::mt19937& rng, PortSide& p) {
    const auto fill = [&rng](void* at, size_t n) {
        for (size_t i = 0; i < n; ++i) static_cast<uint8_t*>(at)[i] = static_cast<uint8_t>(rng());
    };
    fill(&p.index.progress, sizeof p.index.progress);
    fill(&p.index.words, sizeof p.index.words);
    fill(&p.sets.tables, sizeof p.sets.tables);
    fill(p.sets.data.velocity.data(), p.sets.data.velocity.size());
    fill(p.sets.data.key.data(), p.sets.data.key.size());
    fill(&p.state, sizeof p.state);
    p.sets.counts = { static_cast<uint16_t>(rng() % 501), static_cast<uint16_t>(rng() % 501),
                      static_cast<uint16_t>(rng() % 501) };
}
} // namespace

long vc2WaveIndexPart(Vc2Fw& fw, const Vc2Run& run, const std::vector<uint8_t>& index) {
    Vc2WavesTally tally { "index" };
    std::map<int32_t, long> gave;
    if (!index.empty()) {
        PortSide p { {}, {}, {}, gave };
        tally.outcome(parseBoth(fw, p, index, kIndexPath), "the disk image's index");
        std::printf("the disk image's index: %u waves, %u maps, %u samples\n", p.sets.counts.velocity,
                    p.sets.counts.key, p.sets.counts.alias);
    }
    std::mt19937 rng(static_cast<uint32_t>(run.seed));
    for (int c = 0; c < run.cases && !tally.stopped; ++c) {
        PortSide p { {}, {}, {}, gave };
        if (rng() % 2) noise(rng, p);
        const uint32_t parses = 1 + rng() % 3;
        for (uint32_t k = 0; k < parses && !tally.stopped; ++k) {
            const std::vector<uint8_t> file = drawIndex(rng);
            const char* path = rng() % 16 ? kIndexPath : kElsewhere;
            tally.outcome(parseBoth(fw, p, file, path), "case " + std::to_string(c) + ", parse " + std::to_string(k));
        }
    }
    return tally.say("parses (the disk image's index, then " + std::to_string(run.cases) + " cases of drawn ones; "
                     + std::to_string(gave[kVc2IndexParsed]) + " parsed, " + std::to_string(gave[kVc2IndexMissing])
                     + " not there, " + std::to_string(gave[kVc2IndexNotWlf]) + " not WLF0)");
}
#endif
