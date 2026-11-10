// The firmware image the process plays from (R7 of .claude/claude-packs-review.md, v173).  The ports' tables belong to
// the process, and every instance's voices, encodes and firmware calls read them without a lock, so they're written
// once: by the first image that loads, and only once all of it is in does xtSynthRomReady say so.  Another file is
// taken if it holds the same bytes, with nothing written; one that doesn't is refused, saying why, the tables as they
// were.  To play from another image, the host starts again.
#include "XtSynth.h"
#include "XtFiles.h"
#include "XtFirmware.h"
#include "XtSoloEncoder.h"

#include <atomic>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <vector>

namespace {

// The digest of the file's bytes; none if it can't be read whole
std::optional<uint64_t> digestOf(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    const std::streamoff size = f ? static_cast<std::streamoff>(f.tellg()) : -1;
    if (size <= 0) return std::nullopt;
    std::vector<char> bytes(static_cast<size_t>(size));
    f.seekg(0);
    if (!f.read(bytes.data(), size)) return std::nullopt;
    return xtfile::fnv(bytes.data(), bytes.size());
}

// Every port's tables from the image, then what the firmware's own start-up works out; false if a port's won't read
bool loadTables(const char* path) {
    if (!(xtLoadTables(path) && xtMbLoadTables(path) && xtEnvLoadTables(path) && xtLfoLoadTables(path)
          && xtOscLoadTables(path) && xtCosmLoadTables(path) && xtAnalogLoadTables(path) && xtFwOpen(path)))
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

// The image in the tables: loaded by the first file taken, then only compared with.  The lock and what it guards are
// one object, constinit so that it's there before any instance asks, whatever order the plugin's files start in.
class RomInUse {
public:
    bool take(const std::string& path, std::string* why);
    bool ready() const { return ready_.load(std::memory_order_acquire); }

private:
    std::mutex load_;                     // one take at a time; guards the two below
    std::atomic<bool> ready_ { false };   // set once every table is in, and never cleared
    uint64_t digest_ = 0;                 // the image's bytes, by xtfile::fnv
    std::string lastTaken_;               // not read again: a new instance finding the same path costs nothing
};

bool RomInUse::take(const std::string& path, std::string* why) {
    const std::lock_guard<std::mutex> hold(load_);
    if (ready() && path == lastTaken_) return true;
    const std::optional<uint64_t> digest = digestOf(path);
    if (!digest) return false;
    if (ready() && *digest != digest_) {
        if (why)
            *why = "it isn't the image this host plays from, "
                   + std::filesystem::path(xtFwRomPath()).filename().string() + "; restart the host to change images";
        return false;
    }
    if (!ready() && !loadTables(path.c_str())) return false;
    digest_ = *digest;
    lastTaken_ = path;
    ready_.store(true, std::memory_order_release);
    return true;
}

constinit RomInUse g_image;

} // namespace

bool xtSynthSetRom(const std::string& path, std::string* why) { return g_image.take(path, why); }

bool xtSynthRomReady() { return g_image.ready(); }
