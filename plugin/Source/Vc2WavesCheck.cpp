// xt_vc2waves <rom.4> [cases] [seed] (v500): the VC-2's wave loaders against rom.4's own routines in the interpreter,
// their file calls answered alike on both sides from the same files: the index's parse (Vc2WaveIndexCheck.cpp) on the
// disk image's "VC-2 Wave.bin" and drawn ones.  A line a part, its first difference under it with XT_VC2WAVES_FIRST.
// XT_VC2WAVES_MAIN only.
#ifdef XT_VC2WAVES_MAIN
#include "Vc2WavesCheck.h"
#include "XtFiles.h"
#include <algorithm>

using xtfile::le32;

namespace {
constexpr uint32_t kOpen = 0x89336c60, kRead = 0x89336cf0, kSeek = 0x89336db0, kClose = 0x89336ca0;   // [FW]
constexpr size_t kPathMost = 256;
constexpr uint32_t kReadMost = 1u << 24;   // a read the check answers: none of its files is longer
constexpr int32_t kNoFile = -1;
constexpr const char* kDiskImage = "V-SynthXTDiskImage.bin.0.bin";
constexpr std::string_view kLibrary = "WLF08", kVc2Index = "VC2W";   // the VC-2's index's first version names VC2W
constexpr size_t kVersionAt = 0x10, kVersionChars = 16, kSectionsCountAt = 8, kSectionsAt = 0x40, kSectionBytes = 16;

std::string pathAt(Sh4& c, uint32_t at) {
    std::string s;
    for (size_t i = 0; i < kPathMost; ++i) {
        const char ch = static_cast<char>(c.rd8(at + static_cast<uint32_t>(i)));
        if (ch == 0) break;
        s += ch;
    }
    return s;
}

// The disk image beside rom.4: its VC-2 index, a WLF08 library whose first version names VC2W, from its header to the
// end of its last section (as wlflib.py reads it); empty when there is none
std::vector<uint8_t> indexOnDisk(const std::string& rom) {
    const auto slash = rom.find_last_of('/');
    XtDiskImage disk;
    if (!disk.open((slash == std::string::npos ? std::string() : rom.substr(0, slash + 1)) + kDiskImage)) return {};
    const std::vector<uint8_t>& d = disk.bytes();
    const std::string_view all(reinterpret_cast<const char*>(d.data()), d.size());
    for (size_t at = all.find(kLibrary); at != std::string_view::npos; at = all.find(kLibrary, at + 1)) {
        if (at + kSectionsAt > d.size()) break;
        if (all.substr(at + kVersionAt, kVersionChars).find(kVc2Index) == std::string_view::npos) continue;
        size_t end = kSectionsAt;
        for (uint32_t s = 0; s < le32(&d[at + kSectionsCountAt]); ++s) {
            const size_t entry = at + kSectionsAt + kSectionBytes * s;
            end = std::max<size_t>(end, le32(&d[entry + 4]) + le32(&d[entry + 8]));
        }
        const auto from = d.begin() + static_cast<long>(at);
        return { from, from + static_cast<long>(std::min(end, d.size() - at)) };
    }
    return {};
}
} // namespace

Vc2FileList::File* Vc2FileList::of(int32_t handle) {
    const auto k = static_cast<size_t>(handle);
    return handle >= 1 && k <= files.size() ? &files[k - 1] : nullptr;
}

int32_t Vc2FileList::open(const std::string& path) {
    const auto at = std::ranges::find(files, path, &File::path);
    if (at != files.end()) at->at = 0;
    const int32_t handle = at == files.end() ? kNoFile : static_cast<int32_t>(at - files.begin()) + 1;
    log({ 'o', handle, 0, 0, handle, path });
    return handle;
}

uint32_t Vc2FileList::read(int32_t handle, uint8_t* into, uint32_t n) {
    File* f = of(handle);
    const size_t left = f ? f->bytes.size() - std::min(f->at, f->bytes.size()) : 0;
    const auto k = static_cast<uint32_t>(std::min<size_t>(n, left));
    if (f) std::copy_n(f->bytes.data() + f->at, k, into);
    if (f) f->at += k;
    log({ 'r', handle, n, 0, static_cast<int32_t>(k), "" });
    return k;
}

int32_t Vc2FileList::seek(int32_t handle, int32_t offset, int32_t whence) {
    File* f = of(handle);
    if (f) f->at = whence == 0 ? static_cast<uint32_t>(offset) : f->at + static_cast<size_t>(offset);
    const int32_t answer = f ? static_cast<int32_t>(f->at) : kNoFile;
    log({ 's', handle, static_cast<uint32_t>(offset), static_cast<uint32_t>(whence), answer, "" });
    return answer;
}

void Vc2FileList::close(int32_t handle) { log({ 'c', handle, 0, 0, 0, "" }); }

void Vc2FileList::log(Vc2FileCall call) {
    if (watch) call.watched = watch();
    calls.push_back(std::move(call));
}

void vc2HookFiles(Vc2Fw& fw, Vc2FileList& files) {
    fw.cpu.hook(kOpen, [&files](Sh4& c) { c.ret(static_cast<uint32_t>(files.open(pathAt(c, c.r[4])))); });
    fw.cpu.hook(kRead, [&files](Sh4& c) {
        std::vector<uint8_t> got(std::min(c.r[6], kReadMost));
        const uint32_t k = files.read(static_cast<int32_t>(c.r[4]), got.data(), static_cast<uint32_t>(got.size()));
        files.calls.back().a = c.r[6];
        c.write(c.r[5], got.data(), k);
        c.ret(k);
    });
    fw.cpu.hook(kSeek, [&files](Sh4& c) {
        c.ret(static_cast<uint32_t>(files.seek(static_cast<int32_t>(c.r[4]), static_cast<int32_t>(c.r[5]),
                                               static_cast<int32_t>(c.r[6]))));
    });
    fw.cpu.hook(kClose, [&files](Sh4& c) {
        files.close(static_cast<int32_t>(c.r[4]));
        c.ret(0);
    });
}

std::string vc2FileCallsDifference(const std::vector<Vc2FileCall>& mine, const std::vector<Vc2FileCall>& theirs) {
    const auto [a, b] = std::ranges::mismatch(mine, theirs);
    if (a == mine.end() && b == theirs.end()) return "";
    const auto say = [](const auto& it, const auto& end) {
        if (it == end) return std::string("none");
        char t[200];
        std::snprintf(t, sizeof t, "%c handle %d %#x %#x → %d %s, watched %#llx", it->op, it->handle, it->a, it->b,
                      it->answer, it->path.c_str(), static_cast<unsigned long long>(it->watched));
        return std::string(t);
    };
    return "file call " + std::to_string(a - mine.begin()) + ": " + say(a, mine.end()) + ", rom.4's "
           + say(b, theirs.end());
}

std::string vc2BytesDifference(Vc2Fw& fw, uint32_t at, const void* mine, size_t bytes, const char* what) {
    std::vector<uint8_t> theirs(bytes);
    fw.cpu.read(at, theirs.data(), bytes);
    const auto* ours = static_cast<const uint8_t*>(mine);
    for (size_t i = 0; i < bytes; ++i)
        if (theirs[i] != ours[i]) {
            char t[160];
            std::snprintf(t, sizeof t, "%s, byte 0x%08zx: %02x, against %02x", what, at + i, ours[i], theirs[i]);
            return t;
        }
    return "";
}

void Vc2WavesTally::outcome(const std::string& difference, const std::string& where) {
    ++runs;
    if (difference.empty()) return;
    if (!different++ && xtSwitch("XT_VC2WAVES_FIRST")) std::printf("  %s: %s\n", where.c_str(), difference.c_str());
    stopped = xtSwitch("XT_VC2WAVES_STOP") != nullptr;
}

long Vc2WavesTally::say(const std::string& how) const {
    std::printf("%s: %ld %s, %ld different\n", what, runs, how.c_str(), different);
    return different;
}

int main(int argc, char** argv) {
    Vc2Fw fw;
    const auto run = vc2Start(argc, argv, fw, "xt_vc2waves", [](const char*) { return true; });
    if (!run) return 2;
    const std::vector<uint8_t> index = indexOnDisk(argv[1]);
    if (index.empty()) std::printf("xt_vc2waves: no disk image beside rom.4, or no VC-2 index in it\n");
    const long different = vc2WaveIndexPart(fw, *run, index);
    std::printf("xt_vc2waves: %ld different\n", different);
    return different || index.empty() ? 1 : 0;
}
#endif
