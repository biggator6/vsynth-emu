// fwsteps: what each firmware call the plugin makes takes, in the interpreter's steps, against the allowance its
// machine gives it (plugin/Source/XtFwMachine.h, v175).  Built and run by fwsteps.sh, against a copy of Sh4Interp.cpp
// that tells fwstepsCall each call's steps and time (fwsteps_count.py).  Over the firmware's disk image (its patches
// and preset waves), a project's patches and waves, the tables, and encodes of every `every`th preset wave and the six
// longest: SOLO and BACKING at Encode Depth 0, 64 and 127, ENSEMBLE at 64, the band rebuilds, and imports from five
// rates.  One line a kind: its jobs, those that failed, the most steps a call took, the most a sample of its wave took
// past the fixed allowance, and the largest share of its allowance a call used.
//   fwsteps <rom> <disk image> <projects folder> [every]
#include "XtFiles.h"
#include "XtFirmware.h"
#include "XtFwMachine.h"
#include "XtMultiBandPlayer.h"
#include <algorithm>
#include <charconv>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <string>
#include <vector>

namespace {

struct Kind {
    int jobs = 0, failed = 0;
    uint64_t most = 0;           // steps, the most one call took
    double perSample = 0;        // past kCallSteps, the most a sample took
    double share = 0;            // of its allowance, the most a call used
};

// What the calls took: the job's most in one call, every call's steps and seconds, and each kind's
class Tally {
public:
    void call(uint64_t steps, double seconds) {
        most_ = std::max(most_, steps);
        steps_ += steps;
        seconds_ += seconds;
    }
    // One routine's run, given `samples` of a wave (0: a fixed amount of work)
    void job(const std::string& kind, size_t samples, const std::function<bool()>& run) {
        most_ = 0;
        const bool ok = run();
        Kind& k = kinds_[kind];
        ++k.jobs;
        k.failed += ok ? 0 : 1;
        k.most = std::max(k.most, most_);
        const double allowed = (double)xtfw::kCallSteps + (double)xtfw::kStepsPerSample * (double)samples;
        k.share = std::max(k.share, (double)most_ / allowed);
        if (samples) k.perSample = std::max(k.perSample, ((double)most_ - (double)xtfw::kCallSteps) / (double)samples);
    }
    void report(size_t waves, size_t encoded) const {
        std::printf("%zu preset waves, %zu encoded; %.0f M steps/s; allowance %llu steps a call, %llu more a sample\n",
                    waves, encoded, (double)steps_ / seconds_ / 1e6, (unsigned long long)xtfw::kCallSteps,
                    (unsigned long long)xtfw::kStepsPerSample);
        std::printf("%-16s %6s %6s %15s %12s %9s\n", "kind", "jobs", "failed", "most a call", "a sample", "share");
        for (const auto& [name, k] : kinds_)
            std::printf("%-16s %6d %6d %15llu %12.0f %8.1f%%\n", name.c_str(), k.jobs, k.failed,
                        (unsigned long long)k.most, k.perSample, 100.0 * k.share);
    }

private:
    uint64_t most_ = 0, steps_ = 0;
    double seconds_ = 0;
    std::map<std::string, Kind> kinds_;
};

thread_local Tally* tTally = nullptr;   // the tally this thread's calls add to, main's

std::vector<uint8_t> slurp(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    return { std::istreambuf_iterator<char>(f), {} };
}

void patches(Tally& t, const XtDiskImage& disk, const std::vector<std::string>& projectPatches) {
    for (const auto& e : disk.list("Internal.prj/Patch")) {
        if (e.isDir) continue;
        const auto f = disk.read(e);
        XtPatchFile p;
        t.job("patch, image", 0, [&] { return xtPatchRead(f.data(), f.size(), p); });
    }
    for (const auto& path : projectPatches) {
        const auto f = slurp(path);
        XtPatchFile p;
        t.job("patch, project", 0, [&] { return xtPatchRead(f.data(), f.size(), p); });
    }
}

void tables(Tally& t) {
    std::vector<uint32_t> u;
    std::vector<float> f;
    std::vector<uint8_t> slots;
    std::vector<int16_t> sens;
    t.job("tables", 0, [&] { return xtFwEnvKeyScales(u) && xtFwOscTimeKeyScales(f) && xtFwOscWaveGains(f); });
    t.job("tables", 0, [&] { return xtFwOscOutGains(f) && xtFwOscPitchRatios(f) && xtFwCosmKeySquares(f); });
    t.job("tables", 0, [&] { return xtFwMatrixTables(slots, sens); });
}

// The encodes of one wave, as the plugin makes them
void encodes(Tally& t, const XtWaveFile& w) {
    const int16_t* pcm = w.pcm.data();
    const long fr = w.frames;
    const int ch = w.channels;
    const size_t samples = (size_t)fr * (size_t)ch;
    std::vector<uint8_t> vp;
    for (const int depth : { 0, 64, 127 }) {
        t.job("SOLO encode", samples, [&] { return xtFwEncodeSolo(pcm, fr, ch, depth, vp, nullptr, false, nullptr); });
        t.job("BACKING encode", samples, [&] { return xtFwEncodeMulti(3, pcm, fr, ch, vp, depth); });
    }
    t.job("ENSEMBLE encode", samples, [&] { return xtFwEncodeMulti(5, pcm, fr, ch, vp, 64); });
    for (const int type : { 3, 5 }) {
        XtMbBands b;
        t.job("band rebuild", samples, [&] { return xtFwLoadBands(type, pcm, fr, ch, b); });
    }
    for (const int rate : { 8000, 22050, 32000, 48000, 96000 }) {
        std::vector<int16_t> out;
        const size_t outSamples = ((size_t)fr * 44100 / (size_t)rate + 64) * (size_t)ch;
        t.job("import", samples + outSamples, [&] { return xtFwImport(pcm, fr, ch, rate, out); });
    }
}

// The preset waves to encode: every `every`th, and the six longest
std::vector<size_t> picks(const std::vector<XtWaveFile>& waves, int every) {
    std::vector<size_t> byLength(waves.size());
    for (size_t i = 0; i < waves.size(); ++i) byLength[i] = i;
    std::ranges::sort(byLength, [&](size_t a, size_t b) { return waves[a].frames > waves[b].frames; });
    std::vector<size_t> picked;
    for (size_t i = 0; i < waves.size(); i += (size_t)every) picked.push_back(i);
    picked.insert(picked.end(), byLength.begin(), byLength.begin() + (long)std::min<size_t>(6, byLength.size()));
    std::ranges::sort(picked);
    picked.erase(std::unique(picked.begin(), picked.end()), picked.end());
    return picked;
}

} // namespace

// Each call's steps and seconds, from the counting copy of Sh4::call
void fwstepsCall(uint64_t steps, double seconds) {
    if (tTally) tTally->call(steps, seconds);
}

int main(int argc, char** argv) {
    int every = 8;
    if (argc > 4) std::from_chars(argv[4], argv[4] + std::strlen(argv[4]), every);
    if (argc < 4 || every < 1 || !xtFwOpen(argv[1])) {
        std::fprintf(stderr, "usage: fwsteps rom.2.bin disk-image projects-folder [every]\n");
        return 2;
    }
    XtDiskImage disk;
    if (!disk.open(argv[2])) {
        std::fprintf(stderr, "fwsteps: no disk image at %s\n", argv[2]);
        return 1;
    }
    std::vector<std::string> projectPatches, projectWaves;
    namespace fs = std::filesystem;
    for (const auto& e : fs::recursive_directory_iterator(argv[3], fs::directory_options::follow_directory_symlink)) {
        const std::string ext = e.path().extension().string();
        if (ext == ".p00") projectPatches.push_back(e.path().string());
        if (ext == ".wav" || ext == ".WAV") projectWaves.push_back(e.path().string());
    }
    Tally t;
    tTally = &t;
    tables(t);
    patches(t, disk, projectPatches);
    std::vector<XtWaveFile> waves;
    const auto& bytes = disk.bytes();
    for (const size_t at : disk.presetWaves()) {
        XtWaveFile w;
        t.job(".vpw, image", 0, [&] { return xtWaveRead(&bytes[at], bytes.size() - at, w); });
        if (!w.pcm.empty()) waves.push_back(std::move(w));
    }
    for (const auto& path : projectWaves) {
        const auto f = slurp(path);
        XtWaveFile w;
        t.job("wave, project", 0, [&] { return xtWaveRead(f.data(), f.size(), w); });
    }
    const std::vector<size_t> picked = picks(waves, every);
    for (const size_t i : picked) encodes(t, waves[i]);
    t.report(waves.size(), picked.size());
    tTally = nullptr;
    return 0;
}
