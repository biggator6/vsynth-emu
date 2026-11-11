#include "XtFiles.h"
#include "XtFirmware.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

// A patch file into the store, and xt_files' check: its preset waves here, a wave's own chunks in XtWaveFileCheck.cpp,
// its patches in XtPatchStoreCheck.cpp.  The store's layout and the descriptors' reads are XtPatchStore.cpp's (v187), a wave's
// chunks XtWaveFile.cpp's (v169).
namespace {
using xtfile::le32;
using xtfile::be32;
using xtfile::fail;
} // namespace

// ── A patch ──────────────────────────────────────────────────────────────────

namespace {
// What 0x89490430 checks: 0x1e, SVD0 at +2, PATaKATE at +0x10; the offset of the data at +0x18 and its length at +0x1c, big-endian.
bool patchData(const uint8_t* file, size_t size, const uint8_t*& data, size_t& bytes) {
    if (size < 0x20 || file[0] != 0 || file[1] != 0x1e || std::memcmp(file + 2, "SVD", 3) || std::memcmp(file + 0x10, "PATaKATE", 8)) return false;
    const size_t at = be32(file + 0x18);
    if (at > size) return false;
    data = file + at;
    bytes = std::min<size_t>({ (size_t)be32(file + 0x1c), size - at, (size_t)kXtPatchBytes });
    return true;
}
} // namespace

bool xtPatchRead(const uint8_t* file, size_t size, XtPatchFile& out, std::string* why) {
    out = {};
    const uint8_t* data = nullptr; size_t bytes = 0;
    if (!patchData(file, size, data, bytes)) return fail(why, "not a V-Synth patch");
    if (!xtFwPatchStore(data, bytes, out.store)) return fail(why, "the firmware's patch routines failed");
    for (int k = 0; k < 12; ++k) out.name += (char)xtPatchRaw(out.store, 0x103 + k);
    while (!out.name.empty() && out.name.back() == ' ') out.name.pop_back();
    return true;
}

std::string xtPatchName(const uint8_t* file, size_t size) {
    const uint8_t* data = nullptr; size_t bytes = 0;
    if (!patchData(file, size, data, bytes) || bytes < 12) return {};
    std::string name;
    for (int k = 0; k < 12; ++k) {   // 7 bits each from bit 8: the same place in the file's layout as in the store's
        const int bit = 8 + 7 * k, at = bit / 8, shift = bit % 8;
        name += (char)((((data[at] << 8 | data[at + 1]) >> (9 - shift)) & 0x7f));
    }
    while (!name.empty() && name.back() == ' ') name.pop_back();
    return name;
}

// ── The check ────────────────────────────────────────────────────────────────
// xt_files <rom>: on the firmware's disk image, beside the ROM: every preset wave against the firmware's whole-wave routines
// (0x89529c20, 0x89529ce0), and every patch's values against the firmware's reads (0x894817b0, 0x89484310).
//   xt_files <rom> wave <file.vpw> [out.raw]    what a wave holds, and its audio
//   xt_files <rom> patch <file.p00> [lo hi]     a patch's name, and the parameters lo..hi (hex) that aren't their defaults
//   xt_files <rom> extract <n> <out.vpw>        the disk image's nth preset wave as a file
#ifdef XT_FILES_MAIN
#include <fstream>
#include <iterator>

namespace {
using xtfile::fnv;
using Bytes = std::vector<uint8_t>;

// A file given on the command line, read by `read` into `out`: false, and why said, if it isn't one
template <typename T, typename Read> bool readArg(const char* path, T& out, Read read) {
    std::ifstream f(path, std::ios::binary);
    const Bytes d((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    std::string why;
    if (read(d.data(), d.size(), out, &why)) return true;
    std::printf("not read: %s\n", why.c_str());
    return false;
}

// xt_files <rom> wave <file.vpw> [out.raw]
int showWave(int argc, char** argv) {
    XtWaveFile w;
    if (!readArg(argv[3], w, xtWaveRead)) return 1;
    std::printf("%d ch, %ld frames, encode type %d, loop %ld..%ld\n",
                w.channels, w.frames, w.encodeType, w.loopStart, w.loopEnd);
    if (argc > 4)
        std::ofstream(argv[4], std::ios::binary).write((const char*)w.pcm.data(), (std::streamsize)(w.pcm.size() * 2));
    return 0;
}

// xt_files <rom> patch <file.p00> [lo hi]
int showPatch(int argc, char** argv) {
    XtPatchFile p;
    if (!readArg(argv[3], p, xtPatchRead)) return 1;
    std::printf("%s\n", p.name.c_str());
    const int lo = argc > 4 ? (int)std::strtol(argv[4], nullptr, 16) : 0x103;
    const int hi = argc > 5 ? (int)std::strtol(argv[5], nullptr, 16) : 0x428;
    for (int id = lo; id <= hi; ++id) {
        const XtParamDesc* e = xtParamDesc(id);
        if (!e || e->min == e->max || xtPatchSection(id) < 0) continue;
        const int v = xtPatchValue(p.store, id);
        if (v != e->def - e->centre) std::printf("%03x %d\n", id, v);
    }
    return 0;
}

// xt_files <rom> extract <n> <out.vpw>
int extractWave(const Bytes& disk, const std::vector<size_t>& waves, char** argv) {
    const size_t n = (size_t)std::atol(argv[3]);
    if (n >= waves.size()) return 1;
    XtWaveFile w;
    if (!xtWaveRead(&disk[waves[n]], disk.size() - waves[n], w)) return 1;
    std::ofstream(argv[4], std::ios::binary).write((const char*)&disk[waves[n]], (std::streamsize)w.size);
    std::printf("%d ch, %ld frames, encode type %d\n", w.channels, w.frames, w.encodeType);
    return 0;
}

// What the preset waves hold, counted for xt_files' lines
struct WaveTally {
    int mono = 0, stereo = 0, unread = 0, different = 0;
    long total = 0;
    uint64_t hash = 0xcbf29ce484222325ull;
    double edge = 0, inner = 0;
    int types[8] = {};
    int listed = 0, inOrder = 0, listShort = 0, edited[8] = {};     // the waves' vpno
    int withEvents[8] = {}, withAttacks[8] = {};                    // the waves' vpdt events
    uint64_t eventsHash = 1469598103934665603ull;

    void add(const XtWaveFile& w) {
        const bool typed = w.encodeType >= 0 && w.encodeType < 8;
        (w.channels == 2 ? stereo : mono)++;
        total += w.frames;
        if (typed) ++types[w.encodeType];
        if (w.vpnoCount >= 0) {
            ++listed;
            bool order = (int)w.vpno.size() >= w.vpnoCount;
            for (int k = 0; order && k < w.vpnoCount; ++k) order = w.vpno[(size_t)k] == k;
            inOrder += order;
            if (!order && typed) ++edited[w.encodeType];
            listShort += (int)w.vpno.size() < w.vpnoCount;
        }
        if (!w.events.empty() && typed) {
            ++withEvents[w.encodeType];
            withAttacks[w.encodeType] += w.events.size() > 2;
            eventsHash = fnv(w.events.data(), w.events.size() * 4, eventsHash);
        }
        hash = fnv(w.pcm.data(), w.pcm.size() * 2, hash);
        // Nothing at the blocks' edges that isn't everywhere: a wrong predictor would show here
        for (size_t k = (size_t)w.channels; k < w.pcm.size(); ++k) {
            const double step = std::abs((int)w.pcm[k] - (int)w.pcm[k - (size_t)w.channels]);
            ((k / (size_t)w.channels) % 16 == 0 ? edge : inner) += step;
        }
    }

    void print(size_t waves) const {
        std::printf("rdac: %zu waves (%d mono, %d stereo; %d SOLO, %d BACKING, %d ENSEMBLE), %ld frames, %d unread, %d "
                    "different from the firmware's whole-wave routines, a block's edge %.3f of the mean step, "
                    "hash %016llx\n",
                    waves, mono, stereo, types[1], types[3], types[5], total, unread, different,
                    edge * 15.0 / (inner > 0 ? inner : 1.0), (unsigned long long)hash);
        std::printf("vpno: %d waves with a list, %d of them each event in order, the others %d SOLO, %d BACKING, %d "
                    "ENSEMBLE; %d shorter than their count\n",
                    listed, inOrder, edited[1], edited[3], edited[5], listShort);
        std::printf("vpdt: events on %d SOLO, %d BACKING, %d ENSEMBLE waves, of them with attacks %d, %d, %d; "
                    "hash %016llx\n",
                    withEvents[1], withEvents[3], withEvents[5], withAttacks[1], withAttacks[3], withAttacks[5],
                    (unsigned long long)eventsHash);
    }
};

// A preset wave against the firmware's whole-wave routines (0x89529c20, 0x89529ce0), from its raw data chunk
bool sameAsWhole(const Bytes& disk, size_t at, const XtWaveFile& w) {
    size_t i = at;
    const uint8_t* data = nullptr;
    uint32_t nData = 0;
    while (i + 8 <= disk.size() && !data) {
        const uint32_t n = le32(&disk[i + 4]);
        if (!std::memcmp(&disk[i], "data", 4)) {
            data = &disk[i + 8];
            nData = n;
        }
        i += 8 + (size_t)n + (n & 1);
    }
    std::vector<int16_t> whole;
    return data && xtFwRdacWhole(data + 8, nData - 8, w.frames, w.channels, whole) && whole == w.pcm;
}

// The preset waves: what they hold, and each against the whole-wave routines.  Prints three lines; how many are unread or
// differ.
int checkWaves(const Bytes& disk, const std::vector<size_t>& waves, bool first) {
    WaveTally t;
    for (size_t at : waves) {
        XtWaveFile w;
        std::string why;
        if (!xtWaveRead(&disk[at], disk.size() - at, w, &why)) {
            ++t.unread;
            if (first) std::printf("wave at %zu: %s\n", at, why.c_str());
            continue;
        }
        t.add(w);
        if (sameAsWhole(disk, at, w)) continue;
        ++t.different;
        if (first) std::printf("wave at %zu, %d ch: differs from the whole-wave routine\n", at, w.channels);
    }
    t.print(waves.size());
    return t.unread + t.different;
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 2 || !xtFwOpen(argv[1])) {
        std::fprintf(stderr, "usage: xt_files rom.2.bin [wave file | patch file [lo hi] | extract n out]\n");
        return 2;
    }
    const std::string mode = argc > 2 ? argv[2] : "";
    if (mode == "wave" && argc > 3) return showWave(argc, argv);
    if (mode == "patch" && argc > 3) return showPatch(argc, argv);
    // The disk image, beside the ROM
    std::string image = argv[1];
    const auto slash = image.find_last_of('/');
    image = (slash == std::string::npos ? std::string() : image.substr(0, slash + 1)) + "V-SynthXTDiskImage.bin.0.bin";
    XtDiskImage diskImage;
    if (!diskImage.open(image)) {
        std::printf("xt_files: no disk image beside the ROM\n");
        return 1;
    }
    const auto waves = diskImage.presetWaves();
    if (mode == "extract" && argc > 4) return extractWave(diskImage.bytes(), waves, argv);
    const bool first = std::getenv("XT_FILES_FIRST") != nullptr;
    const int wavesWrong = checkWaves(diskImage.bytes(), waves, first);
    XtPatchCheck patches = xtPatchCheckValues(diskImage, first);       // XtPatchStoreCheck.cpp
    patches.different += xtWaveCheckOwn(diskImage.bytes(), waves[0]);   // XtWaveFileCheck.cpp; with the patches', as it was
    xtPatchCheckPrint(patches);
    const int shortBad = xtPatchCheckShort(diskImage);
    return (wavesWrong || patches.different || shortBad) ? 1 : 0;
}
#endif
