// The VC-2's WLF08 wave index read by rom.4's own parser (vc2index.h).
#include "vc2index.h"
#include <algorithm>
#include <cstdio>
#include <memory>

namespace vc2 {

namespace {

constexpr uint32_t kParse = 0x8934d12a;                         // [FW] (state: +0 the handle, +4 a section's offset)
constexpr uint32_t kOpen = 0x89336c60, kRead = 0x89336cf0, kSeek = 0x89336db0, kClose = 0x89336ca0;   // [FW]
constexpr uint32_t kMappedAt = 0x89336d60;   // [FW] (handle) → where the file's bytes are in memory: the kernel's
constexpr uint32_t kStateBytes = 64;
constexpr size_t kPathMost = 64;
constexpr uint32_t kNoHandle = 0xffffffffu;

constexpr uint8_t kUnwritten = 0xff;

// One reader call with out+0 marked first: false on a fault, nothing found, or out+0 left as it was
bool readerRan(Machine& m, uint32_t reader, std::initializer_list<uint32_t> args, uint32_t out) {
    m.cpu.wr8(out, kUnwritten);
    uint32_t found = 0;
    return m.run(reader, args, &found) && found && m.cpu.rd8(out) != kUnwritten;
}

// A wave's or a map's reader at every value: the chosen number (out+6) and its one-split byte (out+0)
bool readerOverValues(Machine& m, uint32_t reader, int number, uint32_t out, std::array<uint16_t, kSplitValues>& chosen,
                      std::array<uint8_t, kSplitValues>& one) {
    for (int v = 0; v < kSplitValues; ++v) {
        if (!readerRan(m, reader, {(uint32_t)number, (uint32_t)v, out}, out)) return false;
        chosen[v] = m.cpu.rd16(out + 6);
        one[v] = m.cpu.rd8(out);
    }
    return true;
}

} // namespace

std::string cString(Machine& m, uint32_t at, size_t most) {
    std::string s;
    for (char c = (char)m.cpu.rd8(at); c && s.size() < most; c = (char)m.cpu.rd8(++at)) s += c;
    return s;
}

void hookFiles(Machine& m, std::vector<HookedFile> list) {
    auto files = std::make_shared<std::vector<HookedFile>>(std::move(list));
    const auto of = [files](uint32_t handle) {
        return handle >= 1 && handle <= files->size() ? (*files)[handle - 1].file : nullptr;
    };
    m.cpu.hook(kOpen, [&m, files](Sh4& c) {
        const std::string path = cString(m, c.r[4], kPathMost);
        const auto at = std::ranges::find(*files, path, &HookedFile::path);
        if (at != files->end()) at->file->at = 0;   // an open starts at the file's first byte
        c.ret(at == files->end() ? kNoHandle : (uint32_t)(at - files->begin()) + 1);
    });
    m.cpu.hook(kRead, [&m, of](Sh4& c) {
        FileImage* f = of(c.r[4]);
        const size_t n = f ? std::min<size_t>(c.r[6], f->bytes.size() - std::min(f->at, f->bytes.size())) : 0;
        if (f) m.cpu.write(c.r[5], f->bytes.data() + f->at, n);
        if (f) f->at += n;
        c.ret((uint32_t)n);
    });
    m.cpu.hook(kSeek, [of](Sh4& c) {
        if (FileImage* f = of(c.r[4])) f->at = c.r[6] == 0 ? c.r[5] : f->at + (int32_t)c.r[5];
        c.ret(0);
    });
    m.cpu.hook(kMappedAt, [of](Sh4& c) { c.ret(of(c.r[4]) ? of(c.r[4])->mapped : 0); });
    m.cpu.hook(kClose, [](Sh4& c) { c.ret(0); });
}

std::optional<int32_t> runParser(Machine& m, const char* who) {
    const uint32_t state = m.alloc(kStateBytes);
    uint32_t parsed = 0;
    if (!m.run(kParse, {state}, &parsed)) {
        std::printf("%s: the parser %s\n", who, m.error().c_str());
        return std::nullopt;
    }
    return (int32_t)parsed;
}

std::optional<int32_t> parseIndex(Machine& m, FileImage& f, const char* who) {
    if (f.bytes.empty()) {
        std::printf("%s: the parser has no file\n", who);
        return std::nullopt;
    }
    answerKernel(m);
    hookFiles(m, {{kIndexPath, &f}});
    return runParser(m, who);
}

std::optional<IndexChoices> indexChoices(Machine& m, uint32_t out) {
    IndexChoices c;
    const int waves = m.cpu.rd16(kWaveCount), maps = m.cpu.rd16(kMapCount), samples = m.cpu.rd16(kSampleCount);
    c.waveMap.resize(waves + 1);
    c.waveOne.resize(waves + 1);
    c.mapSample.resize(maps + 1);
    c.mapOne.resize(maps + 1);
    c.kind.resize(samples + 1);
    for (int w = 1; w <= waves; ++w)
        if (!readerOverValues(m, kWaveSplit, w, out, c.waveMap[w], c.waveOne[w])) return std::nullopt;
    for (int k = 1; k <= maps; ++k)
        if (!readerOverValues(m, kMapSplit, k, out, c.mapSample[k], c.mapOne[k])) return std::nullopt;
    for (int s = 1; s <= samples; ++s) {
        if (!readerRan(m, kSampleInfo, {(uint32_t)s, out}, out)) return std::nullopt;
        c.kind[s] = m.cpu.rd8(out);
    }
    return c;
}

} // namespace vc2
