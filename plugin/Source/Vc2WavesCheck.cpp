// xt_vc2waves <rom.4> [cases] [seed] (v500): the VC-2's wave loaders against rom.4's own routines in the interpreter,
// their file calls answered alike on both sides from the same files: the index's parse (Vc2WaveIndexCheck.cpp) on the
// disk image's "VC-2 Wave.bin" and drawn ones; the store's pool (Vc2WavePoolCheck.cpp, v501) and records
// (Vc2WaveRecordsCheck.cpp, v502) on drawn calls; and (v503, Vc2WaveBootCheck.cpp) rom.4's own boot load from the disk
// image, Vc2WaveBoot, and the port's lookups on what it left against rom.4's.  A line a part, its first difference
// under it with XT_VC2WAVES_FIRST.  XT_VC2WAVES_MAIN only.
#ifdef XT_VC2WAVES_MAIN
#include "Vc2WavesCheck.h"
#include "Vc2BootFiles.h"
#include <algorithm>

namespace {
constexpr uint32_t kOpen = 0x89336c60, kRead = 0x89336cf0, kSeek = 0x89336db0, kClose = 0x89336ca0;   // [FW]
constexpr size_t kPathMost = 256;
constexpr uint32_t kReadMost = 1u << 24;   // a read the check answers: none of its files is longer
constexpr int32_t kNoFile = -1;
constexpr const char* kDiskImage = "V-SynthXTDiskImage.bin.0.bin";

std::string pathAt(Sh4& c, uint32_t at) {
    std::string s;
    for (size_t i = 0; i < kPathMost; ++i) {
        const char ch = static_cast<char>(c.rd8(at + static_cast<uint32_t>(i)));
        if (ch == 0) break;
        s += ch;
    }
    return s;
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

std::string vc2CallOutcome(Sh4& c, bool ok, std::pair<int64_t, int64_t> answers,
                           const std::function<std::string()>& memory) {
    if (!ok) return "a call faults: " + c.error();
    if (answers.first != answers.second)
        return "a call gives " + std::to_string(answers.first) + ", rom.4's " + std::to_string(answers.second);
    return memory();
}

std::string vc2BytesDifference(Sh4& c, uint32_t at, const void* mine, size_t bytes, const char* what) {
    std::vector<uint8_t> theirs(bytes);
    c.read(at, theirs.data(), bytes);
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
    // The disk image beside rom.4, its VC-2 index and the rest of what the loaders read (Vc2DiskBootFiles, v503)
    const std::string rom = argv[1];
    XtDiskImage disk;
    disk.open(rom.substr(0, rom.find_last_of('/') + 1) + kDiskImage);
    const Vc2DiskBootFiles files(disk);
    const std::vector<uint8_t>& index = files.index();
    if (index.empty()) std::printf("xt_vc2waves: no disk image beside rom.4, or no VC-2 index in it\n");
    long different = vc2WaveIndexPart(fw, *run, index) + vc2WavePoolPart(fw, *run) + vc2WaveRecordsPart(fw, *run);
    if (!index.empty()) different += vc2WaveBootPart(rom, *run, files);
    std::printf("xt_vc2waves: %ld different\n", different);
    return different || index.empty() ? 1 : 0;
}
#endif
