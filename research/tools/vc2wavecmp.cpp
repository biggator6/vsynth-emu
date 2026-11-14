// A wave rom.4's store holds against the plugin's own reading of its stream (vc2wavecmp.h).
#include "vc2wavecmp.h"
#include "XtFiles.h"
#include "XtFirmware.h"
#include "XtMultiBandPlayer.h"
#include <cstdio>
#include <cstring>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace vc2 {

namespace {

constexpr uint32_t kChunk = 0x89351ad0, kBand = 0x89351ff0, kFrames = 0x89351a10;   // [FW] the store's
constexpr uint32_t kHeader = 2, kSteps = 3, kLoop = 4, kVpdt = 5;                   // [FW] chunk kinds
constexpr std::array<uint32_t, 6> kBandSlots{0, 2, 3, 4, 5, 6};   // [FW] the store's bands, xtMbBuildBacking's 0…5
constexpr int kBacking = 3, kBlockFrames = 16, kSaid = 3;   // a BACKING wave's vpsc type; an RDAC block; said a sample

enum class Width { u8, s8, s16, u32 };
// A field of a stored chunk, where xtWaveRead reads it in the stream's chunk (XtWaveFile.cpp's readHeader, readLoops,
// readEvents); its value from the plugin's reading at the same place in wanted()
struct Field {
    const char* name;
    uint32_t kind, at;
    Width width;
};
constexpr std::array kFields{
    Field{"vpsc type", kHeader, 1, Width::u8},         Field{"vpsc stereo", kHeader, 2, Width::u8},
    Field{"vpsc frames", kHeader, 4, Width::u32},      Field{"vpsc key", kHeader, 0xa, Width::s8},
    Field{"vpsc cents", kHeader, 0x18, Width::u32},    Field{"vpsc tempo", kHeader, 0x1c, Width::u32},
    Field{"vplp tune", kLoop, 0, Width::u32},          Field{"vplp start", kLoop, 4, Width::u32},
    Field{"vplp end", kLoop, 8, Width::u32},           Field{"vplp back end", kLoop, 0xc, Width::u32},
    Field{"vplp back start", kLoop, 0x10, Width::u32}, Field{"vplp rate", kLoop, 0x14, Width::s16},
    Field{"vplp back rate", kLoop, 0x16, Width::s16},  Field{"vplp mode", kLoop, 0x18, Width::u8},
    Field{"vplp back mode", kLoop, 0x19, Width::u8},   Field{"vplp switch", kLoop, 0x1a, Width::u8},
    Field{"vplp fine", kLoop, 0x1b, Width::s8},        Field{"vpno count", kSteps, 4, Width::u32},
};

// The plugin's reading of each field of kFields, in its order (a stereo flag and the loop switch as the bytes are)
std::array<long, kFields.size()> wanted(const XtWaveFile& w) {
    const auto u32 = [](long v) { return (long)(uint32_t)v; };
    return {w.encodeType,   w.channels == 2 ? 1 : 0, w.frames,    w.originalKey, u32(w.headerCents),
            u32(w.tempo),   u32(w.tune),             w.loopStart, w.loopEnd,     w.bwdEnd,
            w.bwdStart,     w.loopRateFwd,           w.loopRateBwd, w.fwdMode,   w.bwdMode,
            w.loopSwitchDecides ? 1 : 0, w.tuneFine, w.vpnoCount};
}

long readField(Machine& m, uint32_t at, Width w) {
    switch (w) {
        case Width::u8: return m.cpu.rd8(at);
        case Width::s8: return (int8_t)m.cpu.rd8(at);
        case Width::s16: return (int16_t)m.cpu.rd16(at);
        case Width::u32: return (long)m.cpu.rd32(at);
    }
    return 0;
}

// One store call's answer (id, k), or 0 on a fault
uint32_t storeAt(Machine& m, uint32_t routine, uint32_t id, uint32_t k) {
    uint32_t r = 0;
    return m.run(routine, {id, k}, &r) ? r : 0;
}

// What a sample's store holds against the plugin's reading: how many things differ, the first few said
struct Compare {
    Machine& m;
    uint32_t id;
    int differ = 0;
    void say(const std::string& what, long got, long want) {
        if (++differ <= kSaid) std::printf("  sample %u: %s %ld, the plugin's %ld\n", id, what.c_str(), got, want);
    }
    // Words of T from `at` in the store against the plugin's: the first that differs (or −1), and how many
    template <typename T>
    std::pair<long, long> words(const std::string& what, uint32_t at, const std::vector<T>& want) {
        long first = -1, count = 0;
        for (size_t k = 0; at && k < want.size(); ++k) {
            const auto got = (long)(T)(sizeof(T) == 1 ? m.cpu.rd8(at + (uint32_t)k) : m.cpu.rd16(at + 2 * (uint32_t)k));
            if (got == (long)want[k]) continue;
            say(what + " " + std::to_string(k), got, (long)want[k]);
            first = first < 0 ? (long)k : first;
            ++count;
        }
        return {first, count};
    }
};

void compareFields(Compare& c, const XtWaveFile& w, const std::array<uint32_t, kChunkKinds>& chunk) {
    const auto want = wanted(w);
    for (size_t f = 0; f < kFields.size(); ++f) {
        if (!chunk[kFields[f].kind]) continue;
        const long got = readField(c.m, chunk[kFields[f].kind] + kFields[f].at, kFields[f].width);
        if (got != want[f]) c.say(kFields[f].name, got, want[f]);
    }
    c.words("vpno event", chunk[kSteps] ? chunk[kSteps] + 0xc : 0, w.vpno);
    c.words("vpdt byte", chunk[kVpdt], w.vpdt);
}

// The stream's audio decoded over whole blocks by the plugin's decoder: the frames rounded up to 16 where the data chunk
// holds them (the "data" tag whose chunk ends where xtWaveRead says the stream ends), cut back to the frames; xtWaveRead's
// samples when the data isn't RDAC (its first two bytes 0, 8, as 0x8952ee80 checks)
std::optional<std::vector<int16_t>> wholeBlocks(const uint8_t* s, const XtWaveFile& w) {
    for (size_t p = w.size >= 8 ? w.size - 8 : 0; p > 0; --p) {
        if (std::memcmp(s + p, "data", 4) || p + 8 + xtfile::le32(s + p + 4) != w.size) continue;
        const uint8_t* data = s + p + 8;
        const size_t have = w.size - p - 8;
        if (have < 8 || data[0] != 0 || data[1] != 8) return w.pcm;
        const long rounded = (w.frames + kBlockFrames - 1) & ~(long)(kBlockFrames - 1);
        const long frames = (size_t)rounded * (size_t)w.channels <= have - 8 ? rounded : w.frames;
        std::vector<int16_t> pcm;
        if (!xtFwRdacDecode(data + 8, have - 8, frames, w.channels, pcm)) return std::nullopt;
        pcm.resize((size_t)w.frames * (size_t)w.channels);
        return pcm;
    }
    return std::nullopt;
}

// Band 0 against xtWaveRead's samples and against whole blocks, tallied
void compareAudio(Compare& c, const XtWaveFile& w, const std::vector<int16_t>& whole, uint32_t band0, WaveTally& t) {
    const auto read = c.words("sample word", band0, w.pcm);
    const long lastBlock = (w.frames & ~(long)(kBlockFrames - 1)) * w.channels;
    t.partBlocks += w.frames % kBlockFrames ? 1 : 0;
    t.asRead += read.second ? 0 : 1;
    t.lastBlockOnly += read.second && read.first >= lastBlock ? 1 : 0;
    t.before += read.second && read.first < lastBlock ? 1 : 0;
    Compare again{c.m, c.id};
    t.wholeDiffer += again.words("whole-block word", band0, whole).second ? 1 : 0;
}

// A BACKING wave's bands against xtMbBuildBacking's from its audio, tallied
void compareBands(Compare& c, const XtWaveFile& w, const std::vector<int16_t>& whole, WaveTally& t) {
    XtMbBands built;
    ++t.bandWaves;
    if (!xtMbBuildBacking(whole.data(), w.frames, w.channels, built)) {
        c.say("bands built", 0, 1);
        ++t.bandsDiffer;
        return;
    }
    long differ = 0;
    for (size_t k = 0; k < kBandSlots.size(); ++k)
        differ += c.words("band " + std::to_string(kBandSlots[k]) + " word", storeAt(c.m, kBand, c.id, kBandSlots[k]),
                          built.buf[k]).second;
    t.bandsDiffer += differ ? 1 : 0;
}

} // namespace

bool compareStoredWave(Machine& m, uint32_t id, const uint8_t* stream, size_t size, WaveTally& t) {
    XtWaveFile w;
    if (!xtWaveRead(stream, size, w)) return false;
    const std::optional<std::vector<int16_t>> whole = wholeBlocks(stream, w);
    if (!whole) return false;
    std::array<uint32_t, kChunkKinds> chunk{};
    for (uint32_t k = 0; k < kChunkKinds; ++k) t.chunks[k] += (chunk[k] = storeAt(m, kChunk, id, k)) ? 1 : 0;
    for (uint32_t k = 0; k < kStoreBands; ++k) t.bands[k] += storeAt(m, kBand, id, k) ? 1 : 0;
    Compare c{m, id};
    compareFields(c, w, chunk);
    const long frames = (long)storeAt(m, kFrames, id, 0);
    if (frames != w.frames) c.say("frames", frames, w.frames);
    t.fieldsDiffer += c.differ ? 1 : 0;
    Compare audio{m, id};
    if (w.encodeType == kBacking)
        compareBands(audio, w, *whole, t);
    else
        compareAudio(audio, w, *whole, storeAt(m, kBand, id, 0), t);
    return true;
}

void sayTally(const WaveTally& t, int samples) {
    std::printf("of %d samples: %d not read; the chunks' fields and frames differ on %d\n", samples, t.unread,
                t.fieldsDiffer);
    std::printf("band 0 against xtWaveRead's samples: %d equal, %d differ only in their last block of 16 frames, %d "
                "before it (%d end in a part block)\n", t.asRead, t.lastBlockOnly, t.before, t.partBlocks);
    std::printf("band 0 against whole blocks decoded: %d differ; BACKING samples' bands against xtMbBuildBacking's: "
                "%d of %d differ\n", t.wholeDiffer, t.bandsDiffer, t.bandWaves);
    std::printf("held: chunk kinds");
    for (uint32_t k = 0; k < kChunkKinds; ++k) std::printf(" %u ×%ld", k, t.chunks[k]);
    std::printf("; bands");
    for (uint32_t k = 0; k < kStoreBands; ++k) std::printf(" %u ×%ld", k, t.bands[k]);
    std::printf("\n");
}

} // namespace vc2
