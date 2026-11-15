// The firmware images the process plays from (R7 of .claude/claude-packs-review.md, v173; two known images from v198).
// The ports' tables belong to the process, and every instance's voices, encodes and firmware calls read them without a
// lock, so they're written once: by the first image that loads, and only once all of it is in does xtSynthRomReady say
// so.  Another file is taken if it holds the same bytes, with nothing written; one that doesn't is refused, saying why,
// the tables as they were.  To play from another image, the host starts again.  Each image is known by its header's
// sections: the V-Synth's rom.2, whose tables the ports read, and the VC-2's rom.4, held the same way; a file of the
// other image, or of neither, is refused, saying so.
#include "XtSynth.h"
#include "Sh4Interp.h"
#include "Vc2LegacyTables.h"
#include "Vc2Params.h"
#include "Vc2Unvoiced.h"
#include "XtFiles.h"
#include "XtFirmware.h"
#include "XtSoloEncoder.h"

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <vector>

namespace {

// [FW] the images this plugin knows, by their header's sections (HARDWARE_FIRMWARE.md, "A firmware image's own
// header"): the V-Synth XT's rom.2, whose tables the ports read, and the VC-2's rom.4
struct KnownImage {
    const char* name;    // as research/ghidra_scripts/unpack_firmware.py names the update's parts
    const char* plays;   // whose image it is
    Sh4::ImageSections sections;
};
constexpr KnownImage kVSynth {"rom.2", "the V-Synth", {0x8987cb00u, 0x8a01ceb4u, 0x89834018u, 0x8985858cu}};
constexpr KnownImage kVc2 {"rom.4", "the VC-2", {0x8994fc20u, 0x8a1f11c4u, 0x8990c758u, 0x8992e1bcu}};
const KnownImage* const kKnown[] = {&kVSynth, &kVc2};

// What a file is: its bytes' digest, and the known image it is, if it is one
struct ImageFile {
    uint64_t digest = 0;
    const KnownImage* known = nullptr;
};

// None if it can't be read whole
std::optional<ImageFile> imageFileOf(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    const std::streamoff size = f ? static_cast<std::streamoff>(f.tellg()) : -1;
    if (size <= 0) return std::nullopt;
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    f.seekg(0);
    if (!f.read(reinterpret_cast<char*>(bytes.data()), size)) return std::nullopt;
    ImageFile out;
    out.digest = xtfile::fnv(bytes.data(), bytes.size());
    Sh4::ImageSections s;
    if (!Sh4::sectionsOf(bytes.data(), bytes.size(), s)) return out;
    const auto known = std::ranges::find(kKnown, s, &KnownImage::sections);
    out.known = known == std::end(kKnown) ? nullptr : *known;
    return out;
}

// Why a file isn't the image `want`: whose it is, or that it's none this plugin knows
std::string notTheImage(const KnownImage& want, const KnownImage* is) {
    const std::string wanted = std::string(want.plays) + "'s " + want.name;
    if (!is) return "it isn't a firmware image this plugin knows, not " + wanted;
    return "it is " + std::string(is->plays) + "'s image, " + is->name + ", not " + wanted;
}

// Every port's tables from the image, then what the firmware's own start-up works out; false if a port's won't read
bool loadTables(const char* path) {
    if (!(xtLoadTables(path) && xtMbLoadTables(path) && xtEnvLoadTables(path) && xtLfoLoadTables(path)
          && xtOscLoadTables(path) && xtCosmLoadTables(path) && xtAnalogLoadTables(path) && xtMsmLoadTables(path)
          && xtFwOpen(path)))
        return false;
    std::vector<uint32_t> ks;
    if (xtFwEnvKeyScales(ks)) xtEnvSetKeyScales(ks.data());
    std::vector<float> ts;
    if (xtFwOscTimeKeyScales(ts)) xtOscSetTimeKeyScales(ts.data());
    if (xtFwOscWaveGains(ts)) xtOscSetWaveGains(ts.data());
    if (xtFwOscOutGains(ts)) xtOscSetOutGains(ts.data());
    if (xtFwOscPitchRatios(ts)) xtOscSetPitchRatios(ts.data());
    if (xtFwCosmKeySquares(ts)) xtCosmSetKeySquares(ts.data());
    std::vector<uint8_t> mxSlots;
    std::vector<int16_t> mxSens;
    if (xtFwMatrixTables(mxSlots, mxSens)) xtOscSetMatrixTables(mxSlots.data(), mxSens.data());
    return true;
}

// The VC-2's image: its parameters' names, descriptors and value text (Vc2Params.cpp, v199), the unvoiced detector's
// coefficients (Vc2Unvoiced.cpp, v208), LEGACY's analysis's tables (Vc2LegacyAnalysis.cpp, v210); the VC-2's other
// ports' tables go here as they come (the VC-2 mode's step 3), read at this start and never at a switch of modes
bool loadVc2(const char* path) { return vc2LoadParams(path) && vc2LoadUnvoiced(path) && vc2LoadLegacy(path); }

// An image in the tables: loaded by the first file taken, then only compared with.  The lock and what it guards are
// one object, constinit so that it's there before any instance asks, whatever order the plugin's files start in.
class RomInUse {
public:
    constexpr RomInUse(const KnownImage& image, bool (*load)(const char*)) : image_(image), load_(load) {}
    bool take(const std::string& path, std::string* why);
    bool ready() const { return ready_.load(std::memory_order_acquire); }

private:
    const KnownImage& image_;             // the image it holds
    bool (*load_)(const char*);           // its tables from a file of it
    std::mutex lock_;                     // one take at a time; guards the four below
    std::atomic<bool> ready_ { false };   // set once every table is in, and never cleared
    uint64_t digest_ = 0;                 // the image's bytes, by xtfile::fnv
    std::string loaded_;                  // the file the tables came from
    std::string lastTaken_;               // not read again: a new instance finding the same path costs nothing
};

bool RomInUse::take(const std::string& path, std::string* why) {
    const std::lock_guard<std::mutex> hold(lock_);
    if (ready() && path == lastTaken_) return true;
    const std::optional<ImageFile> file = imageFileOf(path);
    if (!file) return false;
    if (file->known != &image_) {
        if (why) *why = notTheImage(image_, file->known);
        return false;
    }
    if (ready() && file->digest != digest_) {
        if (why)
            *why = "it isn't the image this host plays from, "
                   + std::filesystem::path(loaded_).filename().string() + "; restart the host to change images";
        return false;
    }
    if (!ready() && !load_(path.c_str())) return false;
    if (loaded_.empty()) loaded_ = path;
    digest_ = file->digest;
    lastTaken_ = path;
    ready_.store(true, std::memory_order_release);
    return true;
}

constinit RomInUse g_vsynth { kVSynth, loadTables };
constinit RomInUse g_vc2 { kVc2, loadVc2 };

} // namespace

bool xtSynthSetRom(const std::string& path, std::string* why) { return g_vsynth.take(path, why); }

bool xtSynthRomReady() { return g_vsynth.ready(); }

bool xtSynthSetVc2Rom(const std::string& path, std::string* why) { return g_vc2.take(path, why); }

bool xtSynthVc2RomReady() { return g_vc2.ready(); }
