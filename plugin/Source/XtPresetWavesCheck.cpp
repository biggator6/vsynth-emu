// xt_files' check of the preset waves a project's link reaches (v600; XtPresetWaves.h): the V-Synth's library built
// from the disk image, every link file of Internal.prj against the wave it names, and each wave read whole from its
// stream's bytes alone, as the plugin reads it from the image's file.  XT_FILES_MAIN only.
#ifdef XT_FILES_MAIN
#include "XtPresetWaves.h"
#include <array>
#include <cstdio>
#include <set>

namespace {
constexpr const char* kLinks = "Internal.prj/Wave";
constexpr size_t kKinds = 6;   // the encode types a wave can have, 0 LITE to 5 ENSEMBLE

struct LinkTally {
    int files = 0, wrong = 0;
    std::set<int> named;
};

// Internal.prj's link files, each against the wave its number gives: the name it carries must be that wave's
LinkTally checkLinks(const XtDiskImage& disk, const XtPresetWaves& presets, bool first) {
    LinkTally t;
    for (const XtDiskImage::Entry& e : disk.list(kLinks)) {
        if (e.isDir) continue;
        ++t.files;
        const std::vector<uint8_t> f = disk.read(e);
        std::string why;
        const auto link = xtWaveLinkRead(f.data(), f.size(), &why);
        const XtPresetWave* w = link ? presets.wave(link->number) : nullptr;
        if (w && w->name == link->name) {
            t.named.insert(link->number);
            continue;
        }
        ++t.wrong;
        if (first) std::printf("%s: %s\n", e.name.c_str(), link ? "names another wave than its number's" : why.c_str());
    }
    return t;
}
} // namespace

int xtPresetWavesCheck(const XtDiskImage& disk, bool first) {
    XtPresetWaves presets;
    std::string why;
    if (!presets.build(disk, &why)) {
        std::printf("links: %s\n", why.c_str());
        return 1;
    }
    const LinkTally links = checkLinks(disk, presets, first);
    std::array<int, kKinds> kinds {};
    int unread = 0;
    long frames = 0;
    uint64_t hash = xtfile::fnv(nullptr, 0);
    for (int n = 1; n <= presets.count(); ++n) {
        const XtPresetWave& w = *presets.wave(n);
        XtWaveFile wave;
        if (!xtWaveRead(&disk.bytes()[w.at], w.bytes, wave, &why) || wave.encodeType != w.kind
            || static_cast<size_t>(w.kind) >= kKinds) {
            ++unread;
            if (first) std::printf("preset wave %d %s: %s\n", n, w.name.c_str(), why.c_str());
            continue;
        }
        ++kinds[static_cast<size_t>(w.kind)];
        frames += wave.frames;
        hash = xtfile::fnv(&w.at, sizeof w.at, hash);
        hash = xtfile::fnv(wave.pcm.data(), wave.pcm.size() * sizeof(int16_t), hash);
    }
    const XtPresetWave& last = *presets.wave(presets.count());
    std::printf("links: %d preset waves, %d SOLO, %d BACKING, %d ENSEMBLE, %ld frames, the last %s at %#zx; %d link "
                "files in %s naming %zu of them, %d wrong; %d unread; hash %016llx\n",
                presets.count(), kinds[1], kinds[3], kinds[5], frames, last.name.c_str(), last.at, links.files, kLinks,
                links.named.size(), links.wrong, unread, static_cast<unsigned long long>(hash));
    const bool allNamed = links.named.size() == static_cast<size_t>(presets.count());
    return links.wrong + unread + (allNamed ? 0 : 1);
}
#endif
