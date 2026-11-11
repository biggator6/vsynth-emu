// xt_files' own wave (XT_FILES_MAIN; its main is XtFiles.cpp's, v187): a project's wave, a RIFF round the chunks with its
// audio plain samples, and what a wave says of itself, put in its chunks and read back by xtWaveRead (XtWaveFile.cpp).
#ifdef XT_FILES_MAIN
#include "XtFiles.h"
#include <cstdio>
#include <cstring>
#include <vector>

namespace {
using xtfile::le32;
using Bytes = std::vector<uint8_t>;

size_t tagAt(const Bytes& s, const char* tag) {
    for (size_t i = 0; i + 8 < s.size(); ++i)
        if (!std::memcmp(&s[i], tag, 4)) return i;
    return 0;
}
void put32(Bytes& s, size_t a, uint32_t v) {
    for (int k = 0; k < 4; ++k) s[a + (size_t)k] = (uint8_t)(v >> (8 * k));
}
// A data chunk's tag for the wave's samples, then the samples as plain little-endian words, after `out`
void putData(Bytes& out, const XtWaveFile& w) {
    out.insert(out.end(), { 'd', 'a', 't', 'a', 0, 0, 0, 0 });
    put32(out, out.size() - 4, (uint32_t)w.pcm.size() * 2);
    for (int16_t x : w.pcm) {
        out.push_back((uint8_t)x);
        out.push_back((uint8_t)((uint16_t)x >> 8));
    }
}

// Where the stream's chunks stand: vpsc's, vplp's and vpfl's contents, data's and vpno's tags
struct Places { size_t sc, lp, fl, da, no; };

// What a wave says of itself, put in its chunks: three events out of order in its list (2, 0, 1), its key, cents, tempo,
// tuning and loops, and the loops' rates (300 and -343).  False if it has no list to edit.
bool editChunks(Bytes& s, const Places& c) {
    const bool hasList = c.no && (s[c.no + 4] | s[c.no + 5] << 8) >= 18;
    if (hasList) {
        put32(s, c.no + 8 + 4, 3);
        put32(s, c.no + 8 + 12, 2);
        s[c.no + 8 + 16] = 1;
        s[c.no + 8 + 17] = 0;
    }
    s[c.sc + 0xa] = 65;
    static constexpr struct { bool lp; size_t at; uint32_t v; } kWords[] = {
        { false, 0x18, 15 }, { false, 0x1c, 999991 }, { true, 0, (uint32_t)-1234 }, { true, 4, 11 }, { true, 8, 2222 },
        { true, 0xc, 333 }, { true, 0x10, 44 }, { true, 0x14, 0xfea9012c }, { true, 0x18, 0xf9000002 } };
    for (const auto& word : kWords) put32(s, (word.lp ? c.lp : c.sc) + word.at, word.v);
    return hasList;
}

// The edited wave, read back, holds what editChunks put in it, and the rest as `w`
bool readsEdited(const XtWaveFile& b, const XtWaveFile& w) {
    return b.originalKey == 65 && b.headerCents == 15 && b.tempo == 999991 && b.tune == -1234 && b.loopStart == 11
        && b.loopEnd == 2222 && b.bwdEnd == 333 && b.bwdStart == 44 && b.fwdMode == 2 && b.bwdMode == 0
        && !b.loopSwitchDecides && b.tuneFine == -7 && b.loopRateFwd == 300 && b.loopRateBwd == -343
        && b.frames == w.frames && b.channels == w.channels && b.encodeType == w.encodeType
        && b.vpnoCount == 3 && b.vpno.size() >= 3 && b.vpno[0] == 2 && b.vpno[1] == 0 && b.vpno[2] == 1;
}

// The wave as a project's RIFF: the chunks up to data, LINR for RDAC, and the samples
Bytes asRiff(const Bytes& s, const Places& c, const XtWaveFile& w) {
    Bytes riff = { 'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ', 16, 0, 0, 0,
                   1, 0, (uint8_t)w.channels, 0, 0x44, 0xac, 0, 0, 0, 0, 0, 0, 4, 0, 16, 0 };
    Bytes head(s.begin(), s.begin() + (long)c.da);
    std::memcpy(&head[c.fl + 40], "LINR", 4);
    riff.insert(riff.end(), head.begin(), head.end());
    putData(riff, w);
    return riff;
}

// A project's LITE wave: the fmt chunk, vpsc with type 0 and vplp, the samples; no vpfl.  And the same with no vpsc, a
// plain WAV.  How many read wrong.
int liteWrong(const Bytes& riff, const Bytes& s, const XtWaveFile& w) {
    Bytes lite(riff.begin(), riff.begin() + 36);
    for (const char* t : { "vpsc", "vplp" }) {
        const size_t c = tagAt(s, t);
        const uint32_t n = le32(&s[c + 4]);
        lite.insert(lite.end(), s.begin() + (long)c, s.begin() + (long)(c + 8 + n + (n & 1)));
    }
    lite[36 + 8 + 1] = 0;   // vpsc +1: LITE
    putData(lite, w);
    XtWaveFile l;
    const bool read = xtWaveRead(lite.data(), lite.size(), l);
    const int bad = !read || l.encodeType != 0 || l.pcm != w.pcm || l.loopEnd != 2222 || l.tune != -1234
                    || l.frames != w.frames || l.vpnoCount != -1 || !l.vpno.empty();
    Bytes noSc(lite);
    std::memcpy(&noSc[36], "junk", 4);
    return bad + xtWaveRead(noSc.data(), noSc.size(), l);
}

// Plain WAVs aren't this, nor is the RIFF cut short with its data chunk's length saying so: how many read
int notWavesRead(const Bytes& riff, const XtWaveFile& w) {
    XtWaveFile none;
    const Bytes plain = { 'R', 'I', 'F', 'F', 36, 0, 0, 0, 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ', 16, 0, 0, 0,
                          1, 0, 1, 0, 0x44, 0xac, 0, 0, 0, 0, 0, 0, 2, 0, 16, 0, 'd', 'a', 't', 'a', 0, 0, 0, 0 };
    const int bad = xtWaveRead(plain.data(), plain.size(), none);
    const uint32_t less = (uint32_t)w.pcm.size() * 2 - 4;
    Bytes shortRiff(riff.begin(), riff.end() - 4);
    put32(shortRiff, shortRiff.size() - less - 4, less);
    return bad + xtWaveRead(shortRiff.data(), shortRiff.size(), none);
}
} // namespace

int xtWaveCheckOwn(const std::vector<uint8_t>& disk, size_t at) {
    int bad = 0;
    XtWaveFile w;
    if (xtWaveRead(&disk[at], disk.size() - at, w)) {
        Bytes s(disk.begin() + (long)at, disk.begin() + (long)(at + w.size));
        const Places c { tagAt(s, "vpsc") + 8, tagAt(s, "vplp") + 8, tagAt(s, "vpfl") + 8,
                         tagAt(s, "data"), tagAt(s, "vpno") };
        bad += !editChunks(s, c);
        XtWaveFile back;
        bad += !xtWaveRead(s.data(), s.size(), back) || !readsEdited(back, w) || back.pcm != w.pcm;
        const Bytes riff = asRiff(s, c, w);
        bad += !xtWaveRead(riff.data(), riff.size(), back) || !readsEdited(back, w) || back.pcm != w.pcm;
        bad += liteWrong(riff, s, w) + notWavesRead(riff, w);
    } else ++bad;
    std::printf("a wave's own: %d wrong\n", bad);
    return bad;
}
#endif
