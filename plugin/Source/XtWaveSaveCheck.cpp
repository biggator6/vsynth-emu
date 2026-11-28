// xt_files' check of a wave saved (v482; XtFwWaveFile.h), XT_FILES_MAIN only.  Each wave of the user's projects
// (analysis/projects/*/Wave, found from the ROM's place as the disk image is found beside it), its blocks read and
// written again by rom.2's own writer, must come back as its file byte for byte where the XT wrote it: its LIST naming
// the software as rom.2 names it.  The others (V-Producer's, a plain .wav's) are counted as they come back.  Band waves
// (types 3 to 5) are counted apart, their samples coming through their bands' device.
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

// Where two files first differ, for XT_FILES_FIRST
size_t firstDiff(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    return (size_t)(std::mismatch(a.begin(), a.end(), b.begin(), b.end()).first - a.begin());
}

struct SaveTally {
    int waves = 0, unread = 0, bands = 0, xt = 0, xtSame = 0, others = 0, othersSame = 0;
    int lite = 0, solo = 0;   // of the XT's
};

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
    if (xtWaveHasBands(type)) {
        ++t.bands;
        return;
    }
    std::vector<uint8_t> file;
    std::string why;
    const bool same = xtFwWaveFile(w, file, &why) && file == bytes;
    const bool own = xtWrote(w);
    ++(own ? t.xt : t.others);
    (own ? t.xtSame : t.othersSame) += same;
    if (own) (type == 0 ? t.lite : t.solo) += type < 2;
    if (first && own && !same)
        std::printf("%s: %s at byte %zu of %zu\n", path.filename().c_str(), why.empty() ? "differs" : why.c_str(),
                    firstDiff(file, bytes), bytes.size());
}
} // namespace

int xtWaveSaveCheck(const std::string& rom, bool first) {
    SaveTally t;
    for (const fs::path& p : projectWaves(rom)) saveOne(p, first, t);
    std::printf("waves saved: %d of the projects' waves, %d unread, %d band waves not written; the XT's %d (%d LITE, "
                "%d SOLO) %d back as their files; the others %d, %d back\n",
                t.waves, t.unread, t.bands, t.xt, t.lite, t.solo, t.xtSame, t.others, t.othersSame);
    return t.xt - t.xtSame;
}
#endif
