// xt_files' check of a wave saved (v482; XtFwWaveFile.h), XT_FILES_MAIN only.  Each wave of the user's projects
// (analysis/projects/*/Wave, found from the ROM's place as the disk image is found beside it), its blocks read and
// written again by rom.2's own writer, must come back as its file byte for byte where the XT wrote it: its LIST naming
// the software as rom.2 names it.  The others (V-Producer's, a plain .wav's) are counted as they come back.  Band waves
// (types 3 to 5) too from v483, their bands made by rom.2's rebuild and their samples coming through their device.
#ifdef XT_FILES_MAIN
#include "XtFwWaveFile.h"
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string_view>

namespace fs = std::filesystem;

std::vector<fs::path> xtProjectWaves(const std::string& rom, const fs::path& folder) {
    const fs::path projects = fs::path(rom).parent_path().parent_path().parent_path() / "analysis" / "projects";
    std::vector<fs::path> out;
    std::error_code e;
    for (const auto& project : fs::directory_iterator(projects, e))
        for (const auto& f : fs::directory_iterator(project.path() / folder, e))
            if (f.path().extension() == ".wav") out.push_back(f.path());
    std::sort(out.begin(), out.end());
    return out;
}

std::vector<uint8_t> xtFileBytes(const fs::path& path) {
    std::ifstream f(path, std::ios::binary);
    return { std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>() };
}

XtFileDiff xtFileDiff(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    constexpr int kWordsShown = 6;
    XtFileDiff d;
    d.first = (size_t)(std::mismatch(a.begin(), a.end(), b.begin(), b.end()).first - a.begin());
    const size_t n = std::min(a.size(), b.size());
    int shown = 0;
    for (size_t i = d.first; i < n; ++i) {
        if (a[i] == b[i]) continue;
        d.last = i;
        ++d.bytes;
        const size_t w = i & ~(size_t)1;
        if (w + 1 >= n) continue;
        const int x = (int16_t)(a[w] | a[w + 1] << 8), y = (int16_t)(b[w] | b[w + 1] << 8);
        d.most = std::max(d.most, std::abs(x - y));
        if (shown++ < kWordsShown)
            d.words += " " + std::to_string(w) + ":" + std::to_string(x) + "/" + std::to_string(y);
    }
    return d;
}

namespace {
// [FW] the software rom.2 names in a wave's LIST (0x897a8208): a wave whose LIST has it, the XT wrote
constexpr std::string_view kXtSoftware = "V-Synth Ver.";

bool xtWrote(const XtWaveBlocks& w) {
    const auto& list = w.bytes[XtWaveBlocks::kList];
    return list && std::search(list->begin(), list->end(), kXtSoftware.begin(), kXtSoftware.end()) != list->end();
}

struct SaveTally {
    int waves = 0, unread = 0, xt = 0, xtSame = 0, others = 0, othersSame = 0;
    int lite = 0, solo = 0, bands = 0;   // of the XT's
    int stable = 0, most = 0;   // of the XT's band waves not back: saved again as first saved; the most a sample moved
    int looped = 0, loopsBack = 0;   // of those back, with a vplp: its loops set to marks (v486), and written so
    int cut = 0, cutAsLaw = 0;       // of those back but band waves: truncated by rom.2 (v487), and as the law says
    int bandCutRefused = 0;          // and of the band waves back, their Truncate refused (open)
};

// [FW] the vplp's four loop words, +4 … +0x10 (XtWaveFile.cpp's readLoops), in XtWaveLoops' order
constexpr size_t kLoopWordsAt = 4, kLoopWordsEnd = 0x14;

// A wave's frames, from its vpsc (+4)
long framesOf(const XtWaveBlocks& w) {
    const auto& vpsc = *w.bytes[XtWaveBlocks::kVpsc];
    return (long)(vpsc[4] | vpsc[5] << 8 | vpsc[6] << 16 | (uint32_t)vpsc[7] << 24);
}

// A wave's four loop points set by rom.2's own setter to marks, each its own (v486): whether the file then written is
// its file's blocks, the vplp's four words the marks
bool loopsSetAsMarks(const XtWaveBlocks& w, const std::vector<uint8_t>& bytes) {
    const long f = framesOf(w);
    const XtWaveLoops marks { f / 8, f / 2, 3 * f / 4, f / 4 };
    std::vector<uint8_t> file;
    XtWaveBlocks got;
    if (!xtFwWaveFile(bytes, file, nullptr, XtWaveEdits { std::nullopt, marks })) return false;
    if (!xtWaveBlocksOf(file.data(), file.size(), got)) return false;
    XtWaveBlocks want = w;
    std::vector<uint8_t>& vplp = *want.bytes[XtWaveBlocks::kVplp];
    if (vplp.size() < kLoopWordsEnd) return false;
    const std::array<long, 4> words { marks.loopStart, marks.loopEnd, marks.bwdEnd, marks.bwdStart };
    for (size_t i = 0; i < words.size(); ++i)
        for (size_t b = 0; b < 4; ++b) vplp[kLoopWordsAt + 4 * i + b] = (uint8_t)(words[i] >> (8 * b));
    want.bytes[XtWaveBlocks::kVpfl] = got.bytes[XtWaveBlocks::kVpfl];   // the writer's own
    return got.bytes == want.bytes;
}

// A wave saved again from what its first save wrote: whether it comes back as that.  A band wave's bands are made
// from its samples (0x894a1ef0), which come out of them again (0x894a45c0) a step off where the bands its file was
// written from (its encode's) differ from those; then saved again, they stay (v483)
bool savesAgain(const std::vector<uint8_t>& written) {
    std::vector<uint8_t> file;
    return xtFwWaveFile(written, file) && file == written;
}

// A wave's second quarter
XtWaveCut secondQuarter(const XtWaveBlocks& w) {
    return { framesOf(w) / 4, framesOf(w) / 2 };
}

// A wave's second quarter kept by rom.2's Truncate (v487): whether the file written keeps the law read
// ({#wave-truncate-read}): those frames' bytes, both ends; type LITE (vpsc byte 1, 0) with its frames the kept ones;
// no vpno, vpdt or rdmd; the loops the whole wave; vmdl and LIST as they were.  Empty when it does, else what not
std::string cutKeepsLaw(const XtWaveBlocks& w, const std::vector<uint8_t>& bytes) {
    XtWaveFile was;
    std::vector<uint8_t> file;
    std::string why;
    const auto& data = w.bytes[XtWaveBlocks::kData];
    const size_t frameBytes = (*w.bytes[XtWaveBlocks::kVpsc])[2] != 0 ? 4 : 2;
    const XtWaveCut cut = secondQuarter(w);
    if (!data || !xtFwWaveFile(bytes, file, &why, XtWaveEdits { cut, std::nullopt }))
        return why.empty() ? "no data" : why;
    XtWaveBlocks cutBlocks;
    if (!xtWaveBlocksOf(file.data(), file.size(), cutBlocks) || !xtWaveRead(file.data(), file.size(), was))
        return "unread";
    const long kept = cut.to - cut.from + 1;
    const auto& got = *cutBlocks.bytes[XtWaveBlocks::kData];
    const auto from = data->begin() + (long)frameBytes * cut.from;
    if (got.size() != (size_t)kept * frameBytes)
        return "kept " + std::to_string(got.size() / frameBytes) + " frames, not " + std::to_string(kept);
    if (!std::equal(got.begin(), got.end(), from)) {
        const std::vector<uint8_t> want(from, from + (long)got.size());
        const XtFileDiff d = xtFileDiff(got, want);
        return "frames differ from byte " + std::to_string(d.first) + ", " + std::to_string(d.bytes) +
               " bytes, by up to " + std::to_string(d.most) + ":" + d.words;
    }
    if ((*cutBlocks.bytes[XtWaveBlocks::kVpsc])[1] != 0 || was.frames != kept) return "type or length";
    for (const size_t k : { XtWaveBlocks::kVpno, XtWaveBlocks::kVpdt, XtWaveBlocks::kRdmd })
        if (cutBlocks.bytes[k]) return "an encode's block kept";
    if (was.loopStart != 0 || was.loopEnd != kept - 1 || was.bwdEnd != kept - 1 || was.bwdStart != 0) return "loops";
    if (cutBlocks.bytes[XtWaveBlocks::kVmdl] != w.bytes[XtWaveBlocks::kVmdl] ||
        cutBlocks.bytes[XtWaveBlocks::kList] != w.bytes[XtWaveBlocks::kList])
        return "vmdl or LIST";
    return {};
}

// One wave written again: its tally, and where it differs printed when `first` and it is the XT's
void saveOne(const fs::path& path, bool first, SaveTally& t) {
    const std::vector<uint8_t> bytes = xtFileBytes(path);
    ++t.waves;
    XtWaveBlocks w;
    if (!xtWaveBlocksOf(bytes.data(), bytes.size(), w)) {
        ++t.unread;
        return;
    }
    const int type = (*w.bytes[XtWaveBlocks::kVpsc])[1];
    std::vector<uint8_t> file;
    std::string why;
    const bool same = xtFwWaveFile(bytes, file, &why) && file == bytes;
    const bool own = xtWrote(w);
    ++(own ? t.xt : t.others);
    (own ? t.xtSame : t.othersSame) += same;
    if (own) ++(xtWaveHasBands(type) ? t.bands : type == 0 ? t.lite : t.solo);
    if (same && w.bytes[XtWaveBlocks::kVplp]) {
        ++t.looped;
        t.loopsBack += loopsSetAsMarks(w, bytes);
    }
    if (same && !xtWaveHasBands(type)) {
        ++t.cut;
        const std::string cutWhy = cutKeepsLaw(w, bytes);
        t.cutAsLaw += cutWhy.empty();
        if (first && !cutWhy.empty()) std::printf("%s: Truncate: %s\n", path.filename().c_str(), cutWhy.c_str());
    }
    std::vector<uint8_t> cut;
    const XtWaveEdits bandCut { secondQuarter(w), std::nullopt };
    t.bandCutRefused += same && xtWaveHasBands(type) && !xtFwWaveFile(bytes, cut, nullptr, bandCut);
    if (!own || same) return;
    const XtFileDiff d = xtFileDiff(file, bytes);
    if (xtWaveHasBands(type)) {
        t.stable += !file.empty() && savesAgain(file);
        t.most = std::max(t.most, d.most);
    }
    if (first)
        std::printf("%s: %s at byte %zu of %zu, to %zu, %zu bytes, words by up to %d:%s\n", path.filename().c_str(),
                    why.empty() ? "differs" : why.c_str(), d.first, bytes.size(), d.last, d.bytes, d.most,
                    d.words.c_str());
}
} // namespace

int xtWaveSaveCheck(const std::string& rom, bool first) {
    SaveTally t;
    for (const fs::path& p : xtProjectWaves(rom, "Wave")) saveOne(p, first, t);
    std::printf("waves saved: %d of the projects' waves, %d unread; the XT's %d (%d LITE, %d SOLO, %d band) %d back as "
                "their files, of the band waves not %d saved again the same, a sample moved by up to %d; the others "
                "%d, %d back; loops set to marks by the XT's setter on %d, %d so; truncated by the XT's on %d, %d as "
                "its law, the band waves' refused on %d\n",
                t.waves, t.unread, t.xt, t.lite, t.solo, t.bands, t.xtSame, t.stable, t.most, t.others, t.othersSame,
                t.looped, t.loopsBack, t.cut, t.cutAsLaw, t.bandCutRefused);
    return t.xt - t.xtSame + t.looped - t.loopsBack + t.cut - t.cutAsLaw;
}
#endif
