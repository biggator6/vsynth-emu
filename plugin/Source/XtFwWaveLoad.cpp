// A wave loaded as the XT loads one: XtFwWaveLoad.h (v488).
#include "XtFwWaveLoad.h"
#include "XtFwMachine.h"
#include <algorithm>

namespace xtfw {

namespace {
// [FW] the store's heap, made as the store's init makes it (0x894a0840 → 0x8949d990(&heap, bytes), 1 when made) but
// smaller: the XT gives it 48 MB of its pool, more than the machine's RAM, and a wave needs only its own blocks
constexpr uint32_t kHeapInit = 0x8949d990, kStoreHeap = 0x89ce87e8, kStoreHeapBytes = 0xc00000, kMade = 1;
// [FW] the override word every lookup of the store takes over the key it is given when 0 or more: -1, as the init
// leaves it
constexpr uint32_t kOverride = 0x89ce87f8, kNoOverride = 0xffffffff;
// [FW] the loader, (path, key): 1, or the XT's refusal (-60 open, -35 the store full, -39 not PCM …)
constexpr uint32_t kLoader = 0x894939d0, kLoaded = 1;
// [FW] its file calls, rom.2's wrappers: open (path, mode), stat (fd, buffer: the size at word 0), read (fd, buffer,
// count: the count read), seek (fd, offset, whence: 0 from the start, 1 from here; the new place, which it compares),
// close (fd).  Any fd but 100 and 200, the Z: and V: drives.  On every file here the loader seeks from where it is
// only: from the start is answered as the wrapper's contract says, unchecked
struct FileCalls { uint32_t open, stat, read, seek, close, fd; };
constexpr FileCalls kFile { 0x89484820, 0x894847d0, 0x894848b0, 0x89484970, 0x89484860, 3 };
// [FW] the kernel's the loader reaches: the store heap's lock and unlock, a path's base name (path: its name's
// address), a display text
struct LoaderKernel { uint32_t lock, unlock, baseName, display; };
constexpr LoaderKernel kKernel { 0x88008000, 0x88008010, 0x88008c90, 0x88008a90 };
constexpr uint32_t kPathAt = kSamp + 0x200;   // where the harness puts the path

void answerFiles(Sh4& c, WaveIn& in) {
    const auto size = (uint32_t)in.bytes.size();
    c.hook(kFile.open, [](Sh4& s) { s.ret(kFile.fd); });
    c.hook(kFile.stat, [size](Sh4& s) {
        s.wr32(s.r[5], size);
        s.ret(0);
    });
    c.hook(kFile.read, [&in, size](Sh4& s) {
        const uint32_t n = std::min<uint32_t>(s.r[6], size - std::min(in.at, size));
        if (n > 0) s.write(s.r[5], in.bytes.data() + in.at, n);
        in.at += n;
        s.ret(n);
    });
    c.hook(kFile.seek, [&in](Sh4& s) {
        in.at = (s.r[6] == 0 ? 0 : in.at) + s.r[5];
        s.ret(in.at);
    });
    c.hook(kFile.close, [](Sh4& s) { s.ret(0); });
}

void answerLoaderKernel(Sh4& c, const WaveIn& in) {
    const size_t sep = in.path.find_last_of("\\/:");
    const uint32_t name = sep == std::string::npos ? 0 : (uint32_t)sep + 1;
    c.hook(kKernel.lock, [](Sh4& s) { s.ret(0); });
    c.hook(kKernel.unlock, [](Sh4& s) { s.ret(0); });
    c.hook(kKernel.baseName, [name](Sh4& s) { s.ret(s.r[4] + name); });
    c.hook(kKernel.display, [](Sh4& s) { s.ret(0); });
}
} // namespace

bool loadIntoStore(Sh4& c, WaveIn& in, uint32_t key, std::string* why) {
    answerFiles(c, in);
    answerLoaderKernel(c, in);
    c.wr32(kOverride, kNoOverride);
    uint32_t made = 0;
    if (!c.call(kHeapInit, { kStoreHeap, kStoreHeapBytes }, kStack, &made) || made != kMade) {
        if (why) *why = "the store's heap wasn't made: " + c.error();
        return false;
    }
    c.write(kPathAt, in.path.c_str(), in.path.size() + 1);
    uint32_t loaded = 0;
    if (c.call(kLoader, { kPathAt, key }, kStack, &loaded) && loaded == kLoaded) return true;
    if (why) *why = c.error().empty() ? "the XT's loader refuses it: " + std::to_string((int32_t)loaded)
                                      : "the XT's loader failed: " + c.error();
    return false;
}

} // namespace xtfw
