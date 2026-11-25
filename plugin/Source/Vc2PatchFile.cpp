// A .p02 read by rom.4's own routines: see Vc2PatchFile.h (v450).  Every address is rom.4's.
#include "Vc2PatchFile.h"
#include "Sh4Interp.h"
#include "Vc2PatchStore.h"
#include "XtFwMachine.h"
#include <algorithm>
#include <atomic>
#include <mutex>

namespace {

constexpr uint32_t kDefaultPatch = 0x89325300;   // [FW] (1): the default patch, which the reader lays out under a file
constexpr uint32_t kReadFile = 0x893419a0;       // [FW] (file image, path) → 1 read, a negative error (−90 not a patch)
constexpr uint32_t kToStore = 0x89324b20;        // [FW] (store, file image): the file's layout into the store's
// [FW] the OS's file calls the reader makes, each a wrapper sending on by the path's drive or the handle's kind:
// open (path, mode) → a handle, read (handle, to, bytes) → bytes read, seek (handle, at, from) → where, close (handle)
constexpr uint32_t kOpen = 0x89336c60, kRead = 0x89336cf0, kSeek = 0x89336db0, kClose = 0x89336ca0;
constexpr int kHeaders = 32;   // [FW] the reader's two 16-byte reads before the data (0x893419c2, 0x89341a12)
constexpr uint32_t kHandle = 3, kFromStart = 0;
constexpr int kNameFirst = 0x120, kNameChars = 12;   // [FW] Patch Name 1-12
// The machine's memory, as XtFwMachine lays it out: the file image, the store and the path in its samples' room
constexpr uint32_t kFileImage = xtfw::kSamp, kStore = kFileImage + 0x2000, kPath = kStore + 0x2000;

// The image the reads run on: its path, written once by loadVc2 and read with no lock once `kept_` says so
class Image {
public:
    bool keep(const char* path) {
        const std::lock_guard<std::mutex> hold(lock_);
        if (!kept()) {
            path_ = path;
            kept_.store(true, std::memory_order_release);
        }
        return true;
    }
    bool kept() const { return kept_.load(std::memory_order_acquire); }
    const std::string& path() const { return path_; }   // only once kept()

private:
    std::mutex lock_;
    std::atomic<bool> kept_ { false };
    std::string path_;
};
constinit Image g_image;

// The file's bytes as the OS's calls give them to the reader: one file open, read from where it was sought
class FileCalls {
public:
    FileCalls(const uint8_t* bytes, size_t size) : bytes_(bytes), size_(size) {}
    void serve(Sh4& c) {
        c.hook(kOpen, [](Sh4& s) { s.ret(kHandle); });
        c.hook(kRead, [this](Sh4& s) { s.ret(read(s, s.r[5], s.r[6])); });
        c.hook(kSeek, [this](Sh4& s) { s.ret(seek(s.r[5], s.r[6])); });
        c.hook(kClose, [](Sh4& s) { s.ret(0); });
    }

private:
    const uint8_t* bytes_;
    size_t size_, at_ = 0;
    uint32_t read(Sh4& s, uint32_t to, uint32_t bytes) {
        const size_t n = at_ < size_ ? std::min<size_t>(bytes, size_ - at_) : 0;
        if (n) s.write(to, bytes_ + at_, n);
        at_ += n;
        return (uint32_t)n;
    }
    // From the start, the only way the reader seeks; a place past the end may be sought, as on a disk, and reads nothing
    uint32_t seek(uint32_t to, uint32_t from) {
        if (from != kFromStart) return (uint32_t)-1;
        at_ = to;
        return to;
    }
};

bool fail(std::string* why, const char* what) {
    if (why) *why = what;
    return false;
}

} // namespace

bool vc2LoadPatchFiles(const char* path) { return g_image.keep(path); }

bool vc2PatchFilesLoaded() { return g_image.kept(); }

const std::string& vc2PatchFilesRom() {
    static const std::string none;
    return g_image.kept() ? g_image.path() : none;
}

bool vc2PatchRead(const uint8_t* file, size_t size, Vc2PatchFile& out, std::string* why) {
    out = {};
    if (!g_image.kept()) return fail(why, "rom.4 isn't loaded");
    if (!file || size < (size_t)kHeaders) return fail(why, "not a VC-2 patch");
    Sh4 cpu;
    if (!cpu.loadRom(g_image.path().c_str())) return fail(why, "rom.4 can't be read again");
    cpu.mapRam(xtfw::kRam, xtfw::kRamSize);
    cpu.stepLimit = xtfw::kCallSteps;
    FileCalls calls(file, size);
    calls.serve(cpu);
    cpu.write(kPath, "A:\\", 4);
    uint32_t r0 = 0;
    if (!cpu.call(kDefaultPatch, { 1 }, xtfw::kStack) || !cpu.call(kReadFile, { kFileImage, kPath }, xtfw::kStack, &r0))
        return fail(why, "rom.4's patch routines failed");
    if ((int32_t)r0 < 0) return fail(why, "not a VC-2 patch");   // [FW] its caller's test (0x89341e76)
    if (!cpu.call(kToStore, { kStore, kFileImage }, xtfw::kStack)) return fail(why, "rom.4's patch routines failed");
    out.store.resize(kVc2StoreBytes);
    cpu.read(kStore, out.store.data(), out.store.size());
    for (int k = 0; k < kNameChars; ++k) out.name += (char)xtStoreRaw(vc2Store(), out.store, kNameFirst + k);
    while (!out.name.empty() && out.name.back() == ' ') out.name.pop_back();
    return true;
}
