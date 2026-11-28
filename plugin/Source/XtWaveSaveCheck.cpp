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

namespace {
namespace fs = std::filesystem;
// [FW] the software rom.2 names in a wave's LIST (0x897a8208): a wave whose LIST has it, the XT wrote
constexpr std::string_view kXtSoftware = "V-Synth Ver.";

// Every .wav in each project's Wave folder: <repo>/analysis/projects/*/Wave, the ROM being <repo>/firmware/unpacked's
std::vector<fs::path> projectWaves(const std::string& rom) {
    const fs::path projects = fs::path(rom).parent_path().parent_path().parent_path() / "analysis" / "projects";
    std::vector<fs::path> out;
    std::error_code e;
    for (const auto& project : fs::directory_iterator(projects, e))
        for (const auto& f : fs::directory_iterator(project.path() / "Wave", e))
            if (f.path().extension() == ".wav") out.push_back(f.path());
    std::sort(out.begin(), out.end());
    return out;
}

bool xtWrote(const XtWaveBlocks& w) {
    const auto& list = w.bytes[XtWaveBlocks::kList];
    return list && std::search(list->begin(), list->end(), kXtSoftware.begin(), kXtSoftware.end()) != list->end();
}

// How two files differ, for XT_FILES_FIRST: the first and last byte that do, how many, and the most two 16-bit words
// at an even byte differ by (a sample, where the data is)
struct Diff {
    size_t first = 0, last = 0, bytes = 0;
    int most = 0;
    std::string words;   // the first few that differ: at, a's, b's
};
Diff diffOf(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    constexpr int kWordsShown = 6;
    Diff d;
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

struct SaveTally {
    int waves = 0, unread = 0, xt = 0, xtSame = 0, others = 0, othersSame = 0;
    int lite = 0, solo = 0, bands = 0;   // of the XT's
    int stable = 0, most = 0;   // of the XT's band waves not back: saved again as first saved; the most a sample moved
};

// A wave saved again from what its first save wrote: whether it comes back as that.  A band wave's bands are made
// from its samples (0x894a1ef0), which come out of them again (0x894a45c0) a step off where the bands its file was
// written from (its encode's) differ from those; then saved again, they stay (v483)
bool savesAgain(const std::vector<uint8_t>& written) {
    XtWaveBlocks again;
    std::vector<uint8_t> file;
    return xtWaveBlocksOf(written.data(), written.size(), again) && xtFwWaveFile(again, file) && file == written;
}

// One wave written again: its tally, and where it differs printed when `first` and it is the XT's
void saveOne(const fs::path& path, bool first, SaveTally& t) {
    std::ifstream f(path, std::ios::binary);
    const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    ++t.waves;
    XtWaveBlocks w;
    if (!xtWaveBlocksOf(bytes.data(), bytes.size(), w)) {
        ++t.unread;
        return;
    }
    const int type = (*w.bytes[XtWaveBlocks::kVpsc])[1];
    std::vector<uint8_t> file;
    std::string why;
    const bool same = xtFwWaveFile(w, file, &why) && file == bytes;
    const bool own = xtWrote(w);
    ++(own ? t.xt : t.others);
    (own ? t.xtSame : t.othersSame) += same;
    if (own) ++(xtWaveHasBands(type) ? t.bands : type == 0 ? t.lite : t.solo);
    if (!own || same) return;
    const Diff d = diffOf(file, bytes);
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
    for (const fs::path& p : projectWaves(rom)) saveOne(p, first, t);
    std::printf("waves saved: %d of the projects' waves, %d unread; the XT's %d (%d LITE, %d SOLO, %d band) %d back as "
                "their files, of the band waves not %d saved again the same, a sample moved by up to %d; the others "
                "%d, %d back\n",
                t.waves, t.unread, t.xt, t.lite, t.solo, t.bands, t.xtSame, t.stable, t.most, t.others, t.othersSame);
    return t.xt - t.xtSame;
}
#endif
