// xt_vc2synth's reads part (v343): C's reads of the part (Vc2SynthReads) against rom.4's own on its memory, after the
// start and the tones' makes, on the seed after the run's.  First the part's regions apart, none running into the
// next; then every read C makes of the part's tables, globals, voices, key tables, glides, channel states, Ms and trip
// records over their arguments (parts and kinds 0…15, an envelope's 13, an LFO's 5, two COSM inputs, two OSCs), none
// reaching a byte no region holds; and every row's placement of every voice's objects, each index, against its
// allocator's.  Then [cases] cases, each noise in eight drawn regions on both sides (a window of 1 KB in a larger
// one), then 40 events: the part's bytes by address against rom.4's at a drawn place in a region; the Matrix's ten
// readers in a drawn order on a drawn M (a note's in the pool, or the part's own one time in eight), each against
// rom.4's: the sums' place and M's words after; the OSC's two getters on either OSC; a sum at a drawn offset of a
// drawn group.  M is noise but its sources' routines (rom.4's
// by a drawn source parameter, the bend one time in four) and, seven times in eight, a slot's destination within the
// limits; its channel one of the part's first 30 channel states (the sources read 0x184 bytes on), its part 0…15; the
// channel states, G and the trip records noise, the part table its start's.  XT_VC2SYNTH_MAIN only.
#ifdef XT_VC2SYNTH_MAIN
#include "Vc2SynthCheck.h"
#include "Vc2SynthReads.h"
#include <algorithm>
#include <cstring>

namespace {
constexpr int kEvents = 40, kNoised = 8, kStates = 30, kSourceParams = 112, kBendParam = 96;
constexpr uint32_t kWindow = 0x400;   // the noise's most bytes in a region
// [FW] the voices and a call's words in one, the Matrix's settings G, the part table
constexpr uint32_t kVoicesAt = 0x89981b6c, kVoiceWordsAt = 0x34, kGAt = 0x89997398, kPartTableAt = 0x8993d040;

struct ReadsCounts {
    long events = 0, memory = 0, readers = 0, getters = 0, sums = 0, placements = 0, unheld = 0;
};

std::string regionsApart(Vc2Synth& s) {
    std::vector<Vc2SynthRegion> r = vc2SynthRegions(s);
    std::ranges::sort(r, {}, &Vc2SynthRegion::at);
    for (size_t k = 1; k < r.size(); ++k) {
        if (r[k - 1].at + r[k - 1].bytes > r[k].at) return std::string(r[k - 1].what) + " runs into " + r[k].what;
    }
    return "";
}

// Every read of C's tables by part and kind
void partReads(Vc2SynthReads& r) {
    for (uint32_t part = 0; part < 16; ++part) {
        r.commonOf(part);
        r.oscHeader(part);
        r.tripMoving((uint8_t)part);
        for (uint32_t kind = 0; kind < 16; ++kind) {
            r.modeOf(part, kind);
            r.mixOf(part, kind);
            r.tvaOf(part, kind);
            for (uint32_t i = 0; i < 13; ++i) r.envelopeOf(part, kind, i);
            for (uint32_t i = 0; i < 5; ++i) r.lfoOf(part, kind, i);
            for (uint32_t i = 0; i < 2; ++i) r.cosmInputOf(part, kind, i);
            for (uint32_t i = 0; i < 2; ++i) r.oscRecord(part, kind, i);
        }
    }
}

// Every read of the part's objects C makes by address: the voices' words, key tables, glides, channels, Ms; how many
// bytes no region held
long unheldReads(Vc2Synth& s) {
    Vc2SynthReads& r = *s.reads;
    const long before = r.unheld();
    partReads(r);
    r.oscSystems();
    r.tvaGlobals();
    r.clock();
    for (uint32_t k = 0; k < (uint32_t)Vc2PartVoices::kVoices; ++k) {
        r.voiceAt(kVoicesAt + (uint32_t)sizeof(Vc2Voice) * k + kVoiceWordsAt);
    }
    for (uint32_t k = 0; k < (uint32_t)Vc2Keys::kTables; ++k) {
        r.channelOf(Vc2Keys::kTablesAt + (uint32_t)sizeof(Vc2KeyTable) * k);
        r.holdOf(Vc2Keys::kTablesAt + (uint32_t)sizeof(Vc2KeyTable) * k);
    }
    for (uint32_t k = 0; k < (uint32_t)Vc2Keys::kGlides; ++k) {
        r.glideAt(Vc2Keys::kGlidesAt + (uint32_t)sizeof(Vc2KeyGlide) * k);
    }
    for (uint32_t k = 0; k < (uint32_t)kStates; ++k) {
        const uint32_t channel = Vc2OsMidi::kStatesAt + (uint32_t)sizeof(Vc2ChannelState) * k + 2;
        r.bendOf(channel);
        for (uint32_t n = 0; n < 128; ++n) r.controlOf(channel, n);
    }
    for (uint32_t k = 0; k <= (uint32_t)Vc2NoteMatrices::kCount; ++k) {
        const uint32_t m = k < (uint32_t)Vc2NoteMatrices::kCount ? s.sets.matrices.addressOf(k) : Vc2StaticMatrix::kAt;
        r.matrixBend(m);
        for (uint32_t n = 0; n < 5; ++n) r.matrixControl(m, n);
    }
    return r.unheld() - before;
}

// Noise in a drawn region on both sides, a window of it when it's large; never the part table, which the Matrix's
// part source follows to a trip record
void regionNoised(Vc2Fw& fw, const Vc2SynthRegion& r, std::mt19937& rng) {
    if (r.at == kPartTableAt) return;
    const size_t bytes = std::min<size_t>(r.bytes, kWindow);
    const size_t from = r.bytes == bytes ? 0 : rng() % (r.bytes - bytes + 1);
    auto* b = static_cast<uint8_t*>(r.mine) + from;
    for (size_t k = 0; k < bytes; ++k) b[k] = (uint8_t)rng();
    fw.cpu.write(r.at + (uint32_t)from, b, bytes);
}

// The part's bytes at a drawn place in a region, by address, against rom.4's
std::string memoryEvent(Vc2Fw& fw, Vc2SynthMemory& memory, const std::vector<Vc2SynthRegion>& regions,
                        std::mt19937& rng) {
    const Vc2SynthRegion& r = regions[rng() % regions.size()];
    const size_t from = rng() % r.bytes, bytes = 1 + rng() % std::min<size_t>(64, r.bytes - from);
    std::vector<uint8_t> mine(bytes), theirs(bytes);
    memory.read(r.at + (uint32_t)from, mine.data(), bytes);
    fw.cpu.read(r.at + (uint32_t)from, theirs.data(), bytes);
    return mine == theirs ? "" : std::string("the part's bytes in ") + r.what;
}

// A drawn M, as its region; its words, channel and part drawn, the world its sources read noise on both sides
Vc2SynthRegion matrixDrawn(Vc2Fw& fw, Vc2Synth& s, std::mt19937& rng) {
    const uint32_t k = rng() % Vc2NoteMatrices::kCount;
    const bool own = rng() % 8 == 0;
    Vc2MatrixWords& m = own ? s.staticM.m : s.sets.matrices.items[k].m;
    Vc2MatrixLinks& links = own ? s.staticM.links : s.sets.matrices.items[k].links;
    auto* b = reinterpret_cast<uint8_t*>(&m);
    for (size_t i = 0; i < sizeof m; ++i) b[i] = (uint8_t)rng();
    for (uint32_t& routine : m.routines) {
        routine = vc2MatrixSourceRoutine(rng() % 4 == 0 ? kBendParam : (int32_t)(rng() % kSourceParams));
    }
    for (Vc2MatrixSlot& slot : m.slots) {
        if (rng() % 8 != 0) slot.dest = (uint8_t)(rng() % Vc2MatrixTables::kDestinations);
    }
    links.channel = Vc2OsMidi::kStatesAt + (uint32_t)sizeof(Vc2ChannelState) * (rng() % kStates) + 2;
    links.part = (int16_t)(rng() % 16);
    const Vc2SynthRegion world[] = { vc2SynthRegion(Vc2OsMidi::kStatesAt, s.midi.states, ""),
                                     vc2SynthRegion(kGAt, s.g, ""),
                                     vc2SynthRegion(Vc2TripRecords::kAt, s.trips.records, "") };
    for (const Vc2SynthRegion& r : world) {
        auto* w = static_cast<uint8_t*>(r.mine);
        for (size_t i = 0; i < r.bytes; ++i) w[i] = (uint8_t)rng();
        fw.cpu.write(r.at, r.mine, r.bytes);
    }
    const Vc2SynthRegion r = own ? vc2SynthRegion(Vc2StaticMatrix::kAt, s.staticM, "the static M")
                                 : vc2SynthRegion(s.sets.matrices.addressOf(k), s.sets.matrices.items[k], "a note's M");
    fw.cpu.write(r.at, r.mine, r.bytes);
    return r;
}

// rom.4's reader on M: its sums' place, 0 when it faults
uint32_t romGroup(Vc2Fw& fw, uint32_t reader, uint32_t m) {
    uint32_t r0 = 0;
    return fw.call(reader, { m }, &r0) ? r0 : 0;
}

// The ten readers in a drawn order: each one's sums' place, then M's words
std::string readersEvent(Vc2Fw& fw, Vc2Synth& s, const Vc2SynthRegion& m, std::mt19937& rng, ReadsCounts& n) {
    std::array<uint32_t, 10> readers = kVc2MatrixReaders;
    std::shuffle(readers.begin(), readers.end(), rng);
    for (const uint32_t reader : readers) {
        ++n.readers;
        const uint32_t theirs = romGroup(fw, reader, m.at);
        const auto* mine = reinterpret_cast<const uint8_t*>(s.reads->matrixGroup(reader, m.at));
        if (theirs != m.at + (uint32_t)(mine - static_cast<const uint8_t*>(m.mine))) return "a reader's sums' place";
        if (std::string d = vc2SynthBytesDifference(fw, m); !d.empty()) return "after a reader: " + d;
    }
    return "";
}

// The OSC's two getters on a drawn OSC: the group's four sums and its LFO's five, then M's words
std::string gettersEvent(Vc2Fw& fw, Vc2Synth& s, const Vc2SynthRegion& m, std::mt19937& rng, ReadsCounts& n) {
    ++n.getters;
    const uint32_t osc = rng() % 2;
    const Vc2MatrixGroups mine = s.reads->oscGroups(m.at, osc);
    Vc2MatrixGroups theirs {};
    fw.cpu.read(romGroup(fw, kVc2OscGetters[osc], m.at), theirs.osc, sizeof theirs.osc);
    fw.cpu.read(romGroup(fw, kVc2LfoGetters[osc], m.at), theirs.lfo, sizeof theirs.lfo);
    if (std::memcmp(mine.osc, theirs.osc, sizeof mine.osc) != 0) return "the OSC's group";
    if (std::memcmp(mine.lfo, theirs.lfo, sizeof mine.lfo) != 0) return "its LFO's group";
    return vc2SynthBytesDifference(fw, m);
}

// A sum at a drawn even offset into a drawn group
std::string sumEvent(Vc2Fw& fw, Vc2Synth& s, const Vc2SynthRegion& m, std::mt19937& rng, ReadsCounts& n) {
    ++n.sums;
    const uint32_t reader = kVc2MatrixReaders[rng() % kVc2MatrixReaders.size()], offset = 2 * (rng() % 12);
    const int16_t mine = s.reads->matrixSum(reader, m.at, offset);
    const auto theirs = (int16_t)fw.cpu.rd16(romGroup(fw, reader, m.at) + offset);
    return mine == theirs ? vc2SynthBytesDifference(fw, m) : "a sum";
}

// Every row's placement of every voice's objects, each index, against its allocator's
std::string placements(Vc2Fw& fw, Vc2Synth& s, ReadsCounts& n) {
    for (const Vc2SetChild& row : Vc2Sets::children()) {
        for (uint32_t number = 0; number < (uint32_t)Vc2PartVoices::kVoices; ++number) {
            for (uint32_t index = 0; index < row.count; ++index) {
                ++n.placements;
                uint32_t theirs = 0;
                if (!fw.call(row.allocate, { row.bytes, number, index }, &theirs)) return "an allocator faults";
                if (s.reads->allocate(row, number, index) != theirs) return "a placement";
            }
        }
    }
    return "";
}

std::string readsEvent(Vc2Fw& fw, Vc2Synth& s, Vc2SynthMemory& memory, const std::vector<Vc2SynthRegion>& regions,
                       std::mt19937& rng, ReadsCounts& n) {
    ++n.events;
    const uint32_t pick = rng() % 5;
    if (pick < 2) {
        ++n.memory;
        return memoryEvent(fw, memory, regions, rng);
    }
    const Vc2SynthRegion m = matrixDrawn(fw, s, rng);
    if (pick == 2) return readersEvent(fw, s, m, rng, n);
    return pick == 3 ? gettersEvent(fw, s, m, rng, n) : sumEvent(fw, s, m, rng, n);
}
} // namespace

long vc2SynthReadsPart(Vc2Fw& fw, Vc2Synth& s, const Vc2Run& run) {
    ReadsCounts n;
    std::string first = regionsApart(s);
    n.unheld = unheldReads(s);
    if (first.empty() && n.unheld != 0) first = "C's reads reach bytes no region holds";
    if (first.empty()) first = placements(fw, s, n);
    long different = first.empty() ? 0 : 1;
    std::mt19937 rng((uint32_t)run.seed + 1);
    Vc2SynthMemory memory(s);
    const std::vector<Vc2SynthRegion> regions = vc2SynthRegions(s);
    for (int c = 0; c < run.cases; ++c) {
        for (int k = 0; k < kNoised; ++k) regionNoised(fw, regions[rng() % regions.size()], rng);
        std::string d;
        for (int e = 0; e < kEvents && d.empty(); ++e) d = readsEvent(fw, s, memory, regions, rng, n);
        if (d.empty()) continue;
        if (first.empty()) first = "case " + std::to_string(c) + ": " + d;
        ++different;
    }
    if (!first.empty() && xtSwitch("XT_VC2SYNTH_FIRST")) std::printf("  the reads: %s\n", first.c_str());
    std::printf("synth reads: %d cases, %ld events: %ld memory reads, %ld readers, %ld getters, %ld sums, %ld "
                "placements; %ld bytes no region holds\n",
                run.cases, n.events, n.memory, n.readers, n.getters, n.sums, n.placements, n.unheld);
    return different;
}
#endif
